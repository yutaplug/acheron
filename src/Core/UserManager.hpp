#pragma once

#include <QObject>
#include <QHash>
#include <optional>

#include "Snowflake.hpp"
#include "Discord/Entities.hpp"
#include "Storage/UserRepository.hpp"
#include "Storage/MemberRepository.hpp"

namespace Acheron {
namespace Core {

struct MemberKey
{
    Snowflake guildId;
    Snowflake userId;

    bool operator==(const MemberKey &other) const
    {
        return guildId == other.guildId && userId == other.userId;
    }
};

inline size_t qHash(const MemberKey &key, size_t seed = 0)
{
    return qHashMulti(seed, key.guildId, key.userId);
}

class UserManager : public QObject
{
    Q_OBJECT
public:
    explicit UserManager(Snowflake accountId, QObject *parent = nullptr);
    ~UserManager() override;

    // returned pointers are always owned by the underlying ram cache
    [[nodiscard]] Discord::User *getUser(Snowflake userId);
    [[nodiscard]] Discord::Member *getMember(Snowflake guildId, Snowflake userId);

    [[nodiscard]] QString getDisplayName(Snowflake userId, std::optional<Snowflake> guildId = {});

    void saveUser(const Discord::User &user);
    void saveUsers(const QList<Discord::User> &users);
    void saveMember(Snowflake guildId, Snowflake userId, const Discord::Member &member);

    void saveMemberWithUser(Snowflake guildId, const Discord::Member &member);

private:
    QCache<Snowflake, Discord::User> userCache;
    QCache<MemberKey, Discord::Member> memberCache;

    Storage::UserRepository userRepo;
    Storage::MemberRepository memberRepo;
};

} // namespace Core
} // namespace Acheron
