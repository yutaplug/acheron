#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QPalette>
#include <QString>

#include "Core/Theme/Tokens.hpp"

class QJsonObject;

namespace Acheron {
namespace Core {
namespace Theme {

class Manager : public QObject
{
    Q_OBJECT
public:
    static Manager &instance();

    QColor color(Token token) const;

    bool hasOverride(Token token) const;
    void setOverride(Token token, const QColor &color);
    void clearOverride(Token token);
    void resetAll();
    void setOverrides(const QHash<Token, QColor> &overrides);

    QPalette buildPalette() const;
    void apply();

    // load/save only does overrides, export does everything as resolved
    bool load();
    bool save() const;
    bool exportTo(const QString &path) const;
    bool importFrom(const QString &path);

signals:
    void themeChanged();

private:
    Manager() = default;
    Q_DISABLE_COPY(Manager)

    static QString defaultThemePath();
    QJsonObject toObject(bool includeDefaults) const;
    void loadFromObject(const QJsonObject &obj);

    QHash<Token, QColor> overrides;
};

} // namespace Theme
} // namespace Core
} // namespace Acheron
