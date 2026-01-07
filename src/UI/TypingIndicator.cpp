#include "TypingIndicator.hpp"
#include <QHBoxLayout>
#include <QMovie>

namespace Acheron {
namespace UI {

TypingIndicator::TypingIndicator(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(0);

    // dots = new QLabel(this);
    // auto* movie = new QMovie(":/icons/typing.gif");
    // dots->setMovie(movie);
    // movie->start();
    // layout->addWidget(dots);

    label = new ElidedLabel(this);
    label->setStyleSheet("color: #dbdee1; font-weight: bold; font-size: 12px;");
    layout->addWidget(label);

    layout->addStretch();

    setVisible(false);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void TypingIndicator::setTypers(const QStringList &names)
{
    if (names.isEmpty()) {
        setVisible(false);
        return;
    }

    label->setText(formatText(names));
    setVisible(true);
}

QString TypingIndicator::formatText(const QStringList &names)
{
    if (names.isEmpty())
        return "";

    const int count = names.size();

    if (count == 1)
        return tr("%1 is typing").arg(names[0]);

    if (count == 2)
        return tr("%1 and %2 are typing").arg(names[0], names[1]);

    if (count == 3)
        return tr("%1, %2, and %3 are typing").arg(names[0], names[1], names[2]);

    return tr("Several people are typing");
}

} // namespace UI
} // namespace Acheron