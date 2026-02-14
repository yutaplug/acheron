#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLabel>

#include "Core/Snowflake.hpp"

class QToolButton;

namespace Acheron {
namespace UI {

class ChatTextEdit : public QTextEdit
{
    Q_OBJECT
public:
    explicit ChatTextEdit(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *e) override;
signals:
    void returnPressed();
    void escapePressed();
};

class MessageInput : public QWidget
{
    Q_OBJECT
public:
    explicit MessageInput(QWidget *parent = nullptr);
    void clear();
    void setPlaceholder(const QString &name);

    void setReplyTarget(Core::Snowflake messageId, const QString &authorName,
                        const QString &contentSnippet);
    void clearReplyTarget();
    [[nodiscard]] Core::Snowflake replyTargetMessageId() const { return replyMessageId; }

    void setSendBlocked(bool blocked);
    [[nodiscard]] bool isSendBlocked() const { return sendBlocked; }

protected:
    void resizeEvent(QResizeEvent *event) override;

signals:
    void sendMessage(const QString &text);

private:
    ChatTextEdit *textEdit;
    QWidget *replyBar;
    QLabel *replyLabel;
    QToolButton *replyCancelButton;

    Core::Snowflake replyMessageId;
    bool sendBlocked = false;

    void adjustHeight();
};

} // namespace UI
} // namespace Acheron