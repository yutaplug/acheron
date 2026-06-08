#pragma once

#include <QColor>
#include <QHash>
#include <QPushButton>
#include <QWidget>

#include "Core/Theme/Tokens.hpp"

namespace Acheron {
namespace UI {

class AppearancePage : public QWidget
{
    Q_OBJECT
public:
    explicit AppearancePage(QWidget *parent = nullptr);

private:
    void rebuildSwatches();
    void generateInto(const QColor &seed, int schemeIndex, bool dark);

    QHash<Core::Theme::Token, QPushButton *> swatches;

    QColor seedColor;
    QPushButton *seedSwatch = nullptr;
};

} // namespace UI
} // namespace Acheron
