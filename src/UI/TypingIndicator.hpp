#pragma once

#include <QWidget>
#include <QLabel>
#include "ElidedLabel.hpp"

namespace Acheron {
namespace UI {

class TypingIndicator : public QWidget
{
    Q_OBJECT
public:
    explicit TypingIndicator(QWidget *parent = nullptr);

    void setTypers(const QStringList &names);

private:
    ElidedLabel *label;

    QString formatText(const QStringList &names);
};

} // namespace UI
} // namespace Acheron
