#include "Core/Theme/Manager.hpp"

#include "Core/Theme/Stylesheet.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QStyle>

namespace Acheron {
namespace Core {
namespace Theme {

Manager &Manager::instance()
{
    static Manager inst;
    return inst;
}

QColor Manager::color(Token token) const
{
    auto it = overrides.constFind(token);
    if (it != overrides.constEnd())
        return it.value();
    return descriptor(token).defaultColor;
}

bool Manager::hasOverride(Token token) const
{
    return overrides.contains(token);
}

void Manager::setOverride(Token token, const QColor &color)
{
    if (color.isValid())
        overrides.insert(token, color);
}

void Manager::clearOverride(Token token)
{
    overrides.remove(token);
}

void Manager::resetAll()
{
    overrides.clear();
}

void Manager::setOverrides(const QHash<Token, QColor> &overrides)
{
    this->overrides = overrides;
}

QPalette Manager::buildPalette() const
{
    QPalette pal = qApp->style()->standardPalette();

    for (const TokenDescriptor &d : registry()) {
        if (d.role.has_value())
            pal.setColor(*d.role, color(d.token));
    }

    const QColor disabled = color(Token::DisabledText);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    pal.setColor(QPalette::Disabled, QPalette::Text, disabled);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, disabled);
    pal.setColor(QPalette::Disabled, QPalette::PlaceholderText, disabled);

    return pal;
}

void Manager::apply()
{
    qApp->setPalette(buildPalette());
    qApp->setStyleSheet(buildStyleSheet());
    emit themeChanged();
}

QString Manager::defaultThemePath()
{
    const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dirPath);
    if (!dir.exists())
        dir.mkpath(".");
    return dir.filePath("theme.json");
}

QJsonObject Manager::toObject(bool includeDefaults) const
{
    QJsonObject obj;
    obj["_version"] = 1;
    for (const TokenDescriptor &d : registry()) {
        if (!includeDefaults && !overrides.contains(d.token))
            continue;
        const QColor c = color(d.token);
        const QString hex = c.name(c.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb);
        obj[QString::fromUtf8(d.id)] = hex;
    }
    return obj;
}

void Manager::loadFromObject(const QJsonObject &obj)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it.key().startsWith(QLatin1Char('_')))
            continue;
        const TokenDescriptor *d = findById(it.key());
        if (!d)
            continue;
        if (!it.value().isString())
            continue;
        const QColor c(it.value().toString());
        if (!c.isValid())
            continue;
        overrides.insert(d->token, c);
    }
}

bool Manager::load()
{
    overrides.clear();
    QFile file(defaultThemePath());
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    loadFromObject(doc.object());
    return true;
}

bool Manager::save() const
{
    QFile file(defaultThemePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const QJsonDocument doc(toObject(false));
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool Manager::exportTo(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const QJsonDocument doc(toObject(true));
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool Manager::importFrom(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    overrides.clear();
    loadFromObject(doc.object());
    return true;
}

} // namespace Theme
} // namespace Core
} // namespace Acheron
