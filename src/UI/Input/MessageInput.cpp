#include "MessageInput.hpp"
#include "AttachmentPreviewPanel.hpp"

#include "Core/Theme/Icons.hpp"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QAbstractTextDocumentLayout>
#include <QDragEnterEvent>
#include <QImage>
#include <QMimeData>
#include <QPainter>
#include <QPen>
#include <QTimer>
#include <QToolButton>

#include <algorithm>

namespace Acheron {
namespace UI {

ChatTextEdit::ChatTextEdit(QWidget *parent) : QTextEdit(parent)
{
    setObjectName("MessageInput");
    document()->setDocumentMargin(0);
    setAcceptRichText(false);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setPlaceholderText("Message...");
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ChatTextEdit::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (!(e->modifiers() & Qt::ShiftModifier)) {
            emit returnPressed();
            return;
        }
    }
    if (e->key() == Qt::Key_Escape) {
        emit escapePressed();
        return;
    }
    QTextEdit::keyPressEvent(e);
}

static bool mimeHasLocalFiles(const QMimeData *source)
{
    if (!source->hasUrls())
        return false;
    const auto urls = source->urls();
    return std::any_of(urls.begin(), urls.end(),
                       [](const QUrl &url) { return url.isLocalFile(); });
}

bool ChatTextEdit::canInsertFromMimeData(const QMimeData *source) const
{
    return mimeHasLocalFiles(source) || source->hasImage() || QTextEdit::canInsertFromMimeData(source);
}

void ChatTextEdit::insertFromMimeData(const QMimeData *source)
{
    if (mimeHasLocalFiles(source)) {
        emit filesPasted(source->urls());
        return;
    }
    if (source->hasImage()) {
        QImage image = qvariant_cast<QImage>(source->imageData());
        if (!image.isNull()) {
            emit imagePasted(image);
            return;
        }
    }
    QTextEdit::insertFromMimeData(source);
}

MessageInput::MessageInput(QWidget *parent) : QWidget(parent)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(4, 0, 4, 0);
    outerLayout->setSpacing(0);

    // Reply bar
    replyBar = new QWidget(this);
    replyBar->setVisible(false);
    auto *replyLayout = new QHBoxLayout(replyBar);
    replyLayout->setContentsMargins(8, 4, 4, 2);
    replyLayout->setSpacing(4);

    replyMentionButton = new QToolButton(replyBar);
    replyMentionButton->setIcon(Core::Theme::Icons::icon(Core::Theme::Icons::Name::AtSign, Core::Theme::Token::Highlight));
    replyMentionButton->setIconSize(QSize(14, 14));
    replyMentionButton->setFixedSize(20, 20);
    replyMentionButton->setAutoRaise(true);
    replyMentionButton->setCheckable(true);
    replyMentionButton->setChecked(true);
    replyMentionButton->setCursor(Qt::PointingHandCursor);
    replyMentionButton->setToolTip(tr("Mention reply target"));
    replyMentionButton->setStyleSheet("QToolButton { border: none; }");
    replyLayout->addWidget(replyMentionButton);

    replyLabel = new QLabel(replyBar);
    replyLabel->setStyleSheet("color: #b5bac1; font-size: 12px;");
    replyLayout->addWidget(replyLabel, 1);

    replyCancelButton = new QToolButton(replyBar);
    replyCancelButton->setIcon(Core::Theme::Icons::icon(Core::Theme::Icons::Name::X, Core::Theme::Token::PlaceholderText));
    replyCancelButton->setIconSize(QSize(14, 14));
    replyCancelButton->setFixedSize(16, 16);
    replyCancelButton->setAutoRaise(true);
    replyCancelButton->setCursor(Qt::PointingHandCursor);
    replyCancelButton->setStyleSheet("QToolButton { border: none; }");
    replyLayout->addWidget(replyCancelButton);

    connect(replyCancelButton, &QToolButton::clicked, this, &MessageInput::clearReplyTarget);
    connect(replyMentionButton, &QToolButton::toggled, this, [this](bool checked) {
        replyMention = checked;
        updateReplyMentionIcon();
    });

    outerLayout->addWidget(replyBar);

    attachmentPanel = new AttachmentPreviewPanel(this);
    outerLayout->addWidget(attachmentPanel);

    // Text edit
    auto *inputContainer = new QWidget(this);
    auto *inputLayout = new QHBoxLayout(inputContainer);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->setSpacing(4);

    silentTypingButton = new QToolButton(inputContainer);
    silentTypingButton->setIconSize(QSize(18, 18));
    silentTypingButton->setFixedSize(28, 28);
    silentTypingButton->setAutoRaise(true);
    silentTypingButton->setCheckable(true);
    silentTypingButton->setChecked(false);
    silentTypingButton->setCursor(Qt::PointingHandCursor);
    silentTypingButton->setToolTip(tr("Silent typing (typing indicator hidden)"));
    updateSilentTypingIcon();
    inputLayout->addWidget(silentTypingButton);

    textEdit = new ChatTextEdit(inputContainer);
    setFocusProxy(textEdit);

    connect(textEdit, &ChatTextEdit::returnPressed, [this]() {
        if (sendBlocked)
            return;
        QString txt = textEdit->toPlainText().trimmed();
        if (txt.isEmpty() && !attachmentPanel->hasAttachments())
            return;
        emit sendMessage(txt, attachmentPanel->attachments());
        clear();
    });

    connect(textEdit, &ChatTextEdit::escapePressed, this, [this]() {
        clearReplyTarget();
        attachmentPanel->clearAttachments();
    });

    connect(textEdit->document(), &QTextDocument::contentsChanged, this,
            &MessageInput::adjustHeight);

    connect(textEdit->document(), &QTextDocument::contentsChanged, this,
            &MessageInput::updateTypingTimer);

    connect(textEdit, &ChatTextEdit::filesPasted, this, &MessageInput::queueAttachments);
    connect(textEdit, &ChatTextEdit::imagePasted, attachmentPanel, &AttachmentPreviewPanel::addImage);
    connect(attachmentPanel, &AttachmentPreviewPanel::attachmentsChanged, this, &MessageInput::adjustHeight);

    connect(silentTypingButton, &QToolButton::toggled, this, [this](bool checked) {
        silentTyping = checked;
        updateSilentTypingIcon();
        if (checked)
            stopTypingTimer();
        else
            updateTypingTimer();
    });

    // Typing timer
    typingTimer = new QTimer(this);
    typingTimer->setInterval(8000);
    connect(typingTimer, &QTimer::timeout, this, &MessageInput::typingRequired);

    inputLayout->addWidget(textEdit);
    outerLayout->addWidget(inputContainer);

    setAcceptDrops(true);

    adjustHeight();
}

void MessageInput::updateTypingTimer()
{
    if (silentTyping) {
        stopTypingTimer();
        return;
    }
    if (!textEdit->toPlainText().isEmpty())
        startTypingTimer();
    else
        stopTypingTimer();
}

void MessageInput::startTypingTimer()
{
    bool wasActive = typingTimer->isActive();
    typingTimer->start();
    if (!wasActive)
        emit typingRequired();
}

void MessageInput::stopTypingTimer()
{
    typingTimer->stop();
}

void MessageInput::updateSilentTypingIcon()
{
    auto basePm = Core::Theme::Icons::pixmap(
            Core::Theme::Icons::Name::Keyboard, 18,
            silentTyping ? Core::Theme::Token::DisabledText : Core::Theme::Token::PlaceholderText);

    if (silentTyping) {
        QPixmap crossed(basePm.size());
        crossed.fill(Qt::transparent);
        QPainter p(&crossed);
        p.drawPixmap(0, 0, basePm);
        p.setPen(QPen(QColor(255, 80, 80), 2));
        p.drawLine(basePm.width(), 0, 0, basePm.height());
        p.end();
        silentTypingButton->setIcon(QIcon(crossed));
    } else {
        silentTypingButton->setIcon(QIcon(basePm));
    }
}

void MessageInput::updateReplyMentionIcon()
{
    auto basePm = Core::Theme::Icons::pixmap(
            Core::Theme::Icons::Name::AtSign, 14,
            replyMention ? Core::Theme::Token::Highlight : Core::Theme::Token::DisabledText);

    if (!replyMention) {
        QPixmap crossed(basePm.size());
        crossed.fill(Qt::transparent);
        QPainter p(&crossed);
        p.drawPixmap(0, 0, basePm);
        p.setPen(QPen(QColor(255, 80, 80), 2));
        p.drawLine(basePm.width(), 0, 0, basePm.height());
        p.end();
        replyMentionButton->setIcon(QIcon(crossed));
    } else {
        replyMentionButton->setIcon(QIcon(basePm));
    }
}

void MessageInput::setPlaceholder(const QString &name)
{
    textEdit->setPlaceholderText(name);
}

void MessageInput::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void MessageInput::clear()
{
    textEdit->clear();
    clearReplyTarget();
    attachmentPanel->clearAttachments();
    adjustHeight();
    stopTypingTimer();
}

void MessageInput::queueAttachments(const QList<QUrl> &urls)
{
    attachmentPanel->addFiles(urls);
    textEdit->setFocus();
}

void MessageInput::setMaxUploadSize(qint64 bytes)
{
    attachmentPanel->setMaxFileSize(bytes);
}

void MessageInput::dragEnterEvent(QDragEnterEvent *event)
{
    const auto urls = event->mimeData()->urls();
    if (event->mimeData()->hasUrls() &&
        std::any_of(urls.begin(), urls.end(),
                    [](const QUrl &url) { return url.isLocalFile(); }))
        event->acceptProposedAction();
}

void MessageInput::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls())
        return;
    event->acceptProposedAction();
    queueAttachments(event->mimeData()->urls());
}

void MessageInput::setReplyTarget(Core::Snowflake messageId, const QString &authorName,
                                  const QString &contentSnippet)
{
    replyMessageId = messageId;
    replyMention = true;
    replyMentionButton->setChecked(true);
    updateReplyMentionIcon();

    QString snippet = contentSnippet;
    snippet.replace('\n', ' ');
    if (snippet.length() > 100)
        snippet = snippet.left(100) + "...";

    replyLabel->setText(tr("Replying to <b>%1</b> %2").arg(authorName, snippet));
    replyBar->setVisible(true);
    adjustHeight();
    textEdit->setFocus();
}

void MessageInput::clearReplyTarget()
{
    if (!replyMessageId.isValid())
        return;

    replyMessageId = Core::Snowflake::Invalid;
    replyBar->setVisible(false);
    adjustHeight();
}

void MessageInput::setSendBlocked(bool blocked)
{
    sendBlocked = blocked;
}

void MessageInput::insertText(const QString &text)
{
    textEdit->insertPlainText(text);
    textEdit->setFocus();
}

void MessageInput::adjustHeight()
{
    int contentHeight = textEdit->document()->size().height();

    int newHeight = contentHeight + contentHeight;

    if (newHeight < 44)
        newHeight = 44;
    if (newHeight > 200)
        newHeight = 200;

    if (newHeight >= 200)
        textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    else
        textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    textEdit->setFixedHeight(newHeight);

    int totalHeight = newHeight + 12;
    if (replyBar->isVisible())
        totalHeight += replyBar->sizeHint().height();
    if (attachmentPanel->isVisible())
        totalHeight += attachmentPanel->sizeHint().height();

    setFixedHeight(totalHeight);
}

} // namespace UI
} // namespace Acheron
