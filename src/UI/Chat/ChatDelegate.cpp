#include "ChatDelegate.hpp"

#include "ChatModel.hpp"
#include "ChatLayout.hpp"
#include "ChatView.hpp"

namespace Acheron {
namespace UI {

void ChatDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    painter->save();

    const QString content = index.data(ChatModel::ContentRole).toString();
    const QString html = index.data(ChatModel::HtmlRole).toString();
    const QString username = index.data(ChatModel::UsernameRole).toString();
    const QPixmap avatar = qvariant_cast<QPixmap>(index.data(ChatModel::AvatarRole));
    const QDateTime timestamp = index.data(ChatModel::TimestampRole).toDateTime();
    const bool showHeader = index.data(ChatModel::ShowHeaderRole).toBool();
    const bool hasSeparator = index.data(ChatModel::DateSeparatorRole).toBool();

    QFontMetrics fm(option.font);

    const QRect rowRect = option.rect;
    const QRect avatarRect = ChatLayout::avatarRectForRow(rowRect, hasSeparator);
    const QRect headerRect = ChatLayout::headerRectForRow(rowRect, fm, hasSeparator);
    const QRect textRect = ChatLayout::textRectForRow(rowRect, showHeader, fm, hasSeparator);

    if (hasSeparator) {
        QRect separatorRect = ChatLayout::dateSeparatorRectForRow(rowRect);

        painter->setPen(QPen(option.palette.alternateBase().color(), 1));
        int midY = separatorRect.center().y();
        painter->drawLine(separatorRect.left() + 10, midY, separatorRect.right() - 10, midY);

        QString dateText = timestamp.toString("MMMM d, yyyy");

        painter->setFont(option.font);
        QFontMetrics separatorFm(option.font);
        int textWidth = separatorFm.horizontalAdvance(dateText) + 20;
        QRect textBgRect(separatorRect.center().x() - textWidth / 2, separatorRect.top(), textWidth,
                         separatorRect.height());

        painter->fillRect(textBgRect, option.palette.base());

        painter->setPen(option.palette.text().color());
        painter->drawText(separatorRect, Qt::AlignCenter, dateText);
    }

    if (showHeader) {
        if (!avatar.isNull()) {
            painter->drawPixmap(avatarRect, avatar);
        }

        QFont headerFont = option.font;
        headerFont.setBold(true);
        painter->setFont(headerFont);

        QColor headerColor = (option.state & QStyle::State_Selected)
                                     ? option.palette.highlightedText().color()
                                     : option.palette.text().color();
        painter->setPen(headerColor);

        QString header = username + "  " + timestamp.toString("hh:mm");

        painter->drawText(headerRect, Qt::AlignLeft | Qt::AlignTop, header);
    }

    QTextDocument doc;
    ChatLayout::setupDocument(doc, html, option.font, textRect.width());

    painter->translate(textRect.topLeft());

    QAbstractTextDocumentLayout::PaintContext ctx;

    QColor textColor = (option.state & QStyle::State_Selected)
                               ? option.palette.highlightedText().color()
                               : option.palette.text().color();
    ctx.palette.setColor(QPalette::Text, textColor);

    const ChatView *view = qobject_cast<const ChatView *>(option.widget);
    if (view && view->hasTextSelection()) {
        auto start = view->selectionStart();
        auto end = view->selectionEnd();
        int r = index.row();

        if (r >= start.row && r <= end.row) {
            int startChar = 0;
            int endChar = -1;

            if (r == start.row)
                startChar = start.index;
            if (r == end.row)
                endChar = end.index;

            QTextCursor cursor(&doc);
            cursor.setPosition(startChar);

            if (endChar == -1) {
                cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            } else {
                cursor.setPosition(endChar, QTextCursor::KeepAnchor);
            }

            QAbstractTextDocumentLayout::Selection sel;
            sel.cursor = cursor;
            sel.format.setBackground(option.palette.highlight());
            sel.format.setForeground(option.palette.highlightedText());
            ctx.selections.append(sel);
        }
    }

    doc.documentLayout()->draw(painter, ctx);

    // reset translation for attachment rendering
    painter->restore();
    painter->save();

    // render attachments below text
    QList<AttachmentData> attachments =
            index.data(ChatModel::AttachmentsRole).value<QList<AttachmentData>>();
    if (!attachments.isEmpty()) {
        int realTextHeight = int(std::ceil(doc.size().height()));
        int attachmentTop = textRect.top() + realTextHeight + ChatLayout::padding();

        ChatLayout::AttachmentGridLayout grid =
                ChatLayout::calculateAttachmentGrid(attachments.size(), textRect.width());

        bool isSingleImage = (attachments.size() == 1);

        for (const auto &cell : grid.cells) {
            if (cell.attachmentIndex >= attachments.size())
                continue;

            const auto &att = attachments[cell.attachmentIndex];

            QRect imgRect = cell.rect.translated(textRect.left(), attachmentTop);

            if (!att.pixmap.isNull()) {
                if (isSingleImage) {
                    QRect actualRect(imgRect.x(), imgRect.y(), att.displaySize.width(),
                                     att.displaySize.height());
                    painter->drawPixmap(actualRect, att.pixmap);
                } else {
                    QSize pixSize = att.pixmap.size() / att.pixmap.devicePixelRatio();
                    QRect sourceRect;

                    qreal targetAspect = qreal(imgRect.width()) / imgRect.height();
                    qreal pixAspect = qreal(pixSize.width()) / pixSize.height();

                    // crop to fit
                    if (pixAspect > targetAspect) {
                        int cropWidth = qRound(pixSize.height() * targetAspect);
                        int cropX = (pixSize.width() - cropWidth) / 2;
                        sourceRect = QRect(cropX, 0, cropWidth, pixSize.height());
                    } else {
                        int cropHeight = qRound(pixSize.width() / targetAspect);
                        int cropY = (pixSize.height() - cropHeight) / 2;
                        sourceRect = QRect(0, cropY, pixSize.width(), cropHeight);
                    }

                    qreal dpr = att.pixmap.devicePixelRatio();
                    QRect physicalSourceRect(
                            qRound(sourceRect.x() * dpr), qRound(sourceRect.y() * dpr),
                            qRound(sourceRect.width() * dpr), qRound(sourceRect.height() * dpr));

                    painter->drawPixmap(imgRect, att.pixmap, physicalSourceRect);
                }
            } else {
                painter->fillRect(imgRect, QColor(60, 60, 60));
                painter->setPen(option.palette.text().color());
                painter->drawText(imgRect, Qt::AlignCenter, "Loading...");
            }
        }
    }

    painter->restore();

    // correct height calculation
    {
        int realHeight = int(std::ceil(doc.size().height()));

        int requiredHeight = 0;
        QFontMetrics fm(option.font);
        constexpr int pad = ChatLayout::padding();
        constexpr int aSz = ChatLayout::avatarSize();

        if (showHeader) {
            int contentHeight = pad + fm.height() + realHeight + pad;
            int minHeight = pad + aSz + pad;
            requiredHeight = std::max(contentHeight, minHeight);
        } else {
            requiredHeight = realHeight + pad + 1;
        }

        if (hasSeparator) {
            requiredHeight += ChatLayout::separatorHeight();
        }

        // add height for attachments
        QList<AttachmentData> atts =
                index.data(ChatModel::AttachmentsRole).value<QList<AttachmentData>>();
        if (!atts.isEmpty()) {
            if (atts.size() == 1) {
                requiredHeight += atts[0].displaySize.height() + pad;
            } else {
                ChatLayout::AttachmentGridLayout grid = ChatLayout::calculateAttachmentGrid(
                        atts.size(), option.rect.width() - pad - aSz - pad - pad);
                requiredHeight += grid.totalHeight + pad;
            }
        }

        if (option.rect.height() != requiredHeight) {

            auto model = const_cast<QAbstractItemModel *>(index.model());
            QSize exactSize(option.rect.width(), requiredHeight);

            QSize currentCache = index.data(ChatModel::CachedSizeRole).toSize();

            if (currentCache != exactSize) {
                model->setData(index, exactSize, ChatModel::CachedSizeRole);

                QMetaObject::invokeMethod(model, "triggerResize", Qt::QueuedConnection,
                                          Q_ARG(int, index.row()));
            }
        }
    }
}

QSize ChatDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    int viewportWidth = 400;
    if (option.widget) {
        if (auto view = qobject_cast<const QAbstractItemView *>(option.widget))
            viewportWidth = view->viewport()->width();
        else
            viewportWidth = option.widget->width();
    }

    QSize cached = index.data(ChatModel::CachedSizeRole).toSize();
    if (cached.isValid() && cached.width() == viewportWidth) {
        return cached;
    }

    const QString content = index.data(ChatModel::ContentRole).toString();
    const bool showHeader = index.data(ChatModel::ShowHeaderRole).toBool();
    const bool hasSeparator = index.data(ChatModel::DateSeparatorRole).toBool();

    QFontMetrics fm(option.font);
    constexpr int pad = ChatLayout::padding();
    constexpr int aSz = ChatLayout::avatarSize();

    int textLeft = pad + aSz + pad;
    int textWidth = viewportWidth - textLeft - pad;
    if (textWidth < 10)
        textWidth = 10;

    int approxCharWidth = fm.averageCharWidth();
    int totalTextPixels = content.length() * approxCharWidth;
    int approxLines = (totalTextPixels / textWidth) + 1;

    int textHeight = approxLines * fm.lineSpacing();

    int totalHeight = 0;
    if (showHeader) {
        int contentHeight = pad + fm.height() + textHeight + pad;
        int minHeight = pad + aSz + pad;
        totalHeight = std::max(contentHeight, minHeight);
    } else {
        totalHeight = textHeight + pad + 1;
    }

    if (hasSeparator) {
        totalHeight += ChatLayout::separatorHeight();
    }

    // add height for attachments
    QList<AttachmentData> attachments =
            index.data(ChatModel::AttachmentsRole).value<QList<AttachmentData>>();
    if (!attachments.isEmpty()) {
        if (attachments.size() == 1) {
            totalHeight += attachments[0].displaySize.height() + pad;
        } else {
            ChatLayout::AttachmentGridLayout grid =
                    ChatLayout::calculateAttachmentGrid(attachments.size(), textWidth);
            totalHeight += grid.totalHeight + pad;
        }
    }

    return QSize(viewportWidth, totalHeight);
}

} // namespace UI
} // namespace Acheron