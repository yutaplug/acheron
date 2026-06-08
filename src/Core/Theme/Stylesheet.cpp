#include "Core/Theme/Stylesheet.hpp"

#include "Core/Theme/Manager.hpp"
#include "Core/Theme/Tokens.hpp"

#include <QColor>

namespace Acheron {
namespace Core {
namespace Theme {

namespace {
QString hex(const QColor &c)
{
    return c.name(QColor::HexRgb);
}
} // namespace

QString buildStyleSheet()
{
    const Manager &m = Manager::instance();

    const QColor windowBg = m.color(Token::WindowBg);
    const QColor baseBg = m.color(Token::BaseBg);
    const QColor altBg = m.color(Token::AlternateBaseBg);
    const QColor tooltipBg = m.color(Token::TooltipBg);
    const QColor windowText = m.color(Token::WindowText);
    const QColor tooltipText = m.color(Token::TooltipText);
    const QColor divider = m.color(Token::Divider);
    const QColor highlight = m.color(Token::Highlight);
    const QColor highlightText = m.color(Token::HighlightedText);

    const QColor accentMuted = highlight.darker(120);

    QString qss;

    qss += QStringLiteral("QWidget {"
                          "  background-color: %1;"
                          "  color: %2;"
                          "  selection-background-color: %3;"
                          "  selection-color: %4;"
                          "}")
                   .arg(hex(windowBg), hex(windowText), hex(highlight), hex(highlightText));

    qss += QStringLiteral("QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox,"
                          " QComboBox {"
                          "  background-color: %1;"
                          "  border: 1px solid %2;"
                          "  border-radius: 4px;"
                          "  padding: 5px;"
                          "}"
                          "QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus,"
                          " QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {"
                          "  border-color: %3;"
                          "}")
                   .arg(hex(baseBg), hex(divider), hex(highlight));

    qss += QStringLiteral("QComboBox::drop-down { border: none; width: 20px; }"
                          "QComboBox QAbstractItemView {"
                          "  background-color: %1;"
                          "  border: 1px solid %2;"
                          "  selection-background-color: %3;"
                          "}")
                   .arg(hex(altBg), hex(divider), hex(highlight));

    qss += QStringLiteral("QCheckBox::indicator, QRadioButton::indicator {"
                          "  width: 14px;"
                          "  height: 14px;"
                          "  border: 1px solid %1;"
                          "  background-color: %2;"
                          "}"
                          "QCheckBox::indicator:checked, QRadioButton::indicator:checked {"
                          "  background-color: %3; border-color: %3;"
                          "}")
                   .arg(hex(divider), hex(baseBg), hex(highlight));

    qss += QStringLiteral("QScrollBar:vertical, QScrollBar:horizontal {"
                          "  background-color: %1; border: none; margin: 0px;"
                          "}"
                          "QScrollBar::handle { background-color: %2; border-radius: 4px; }"
                          "QScrollBar::handle:hover { background-color: %3; }"
                          "QScrollBar::add-line, QScrollBar::sub-line { height: 0px; width: 0px; }")
                   .arg(hex(windowBg), hex(divider), hex(accentMuted));

    qss += QStringLiteral("QToolTip {"
                          "  background-color: %1;"
                          "  color: %2;"
                          "  border: 1px solid %3;"
                          "  padding: 4px;"
                          "}")
                   .arg(hex(tooltipBg), hex(tooltipText), hex(accentMuted));

    qss += "#MemberList QScrollBar::handle:vertical { min-height: 40px; }";

    return qss;
}

} // namespace Theme
} // namespace Core
} // namespace Acheron
