#include "Snowflake.hpp"

#include <QDateTime>
#include <QRandomGenerator>

namespace Acheron {
namespace Core {

constexpr quint64 DISCORD_EPOCH = 1420070400000ULL;

const Snowflake Snowflake::Invalid = -1ULL;

Snowflake::Snowflake() : value(Invalid) { }

Snowflake::Snowflake(quint64 n) : value(n) { }

bool Snowflake::isValid() const noexcept
{
    return *this != Invalid;
}

QString Snowflake::toString() const noexcept
{
    return QString::number(value);
}

Snowflake::operator quint64() const noexcept
{
    return value;
}

Snowflake Snowflake::generateNonce()
{
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    quint64 nonce = static_cast<quint64>(timestamp - DISCORD_EPOCH) << 22;
    // discord does nothing to the lower 22 bits
    return Snowflake(nonce);
}

} // namespace Core
} // namespace Acheron