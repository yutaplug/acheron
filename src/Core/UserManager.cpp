#include "UserManager.hpp"

#include "Logging.hpp"

namespace Acheron {
namespace Core {

UserManager::UserManager(Snowflake accountId, QObject *parent)
    : QObject(parent), userRepo(accountId), memberRepo(accountId)
{
}

UserManager::~UserManager() { }

Discord::User *UserManager::getUser(Snowflake userId)
{
    if (auto *user = userCache.object(userId))
        return user;

    auto dbUser = userRepo.getUser(userId);
    if (dbUser.has_value()) {
        auto *user = new Discord::User(dbUser.value());
        userCache.insert(userId, user);
        return user;
    }

    return nullptr;
}

Discord::Member *UserManager::getMember(Snowflake guildId, Snowflake userId)
{
    MemberKey key{ guildId, userId };

    if (auto *member = memberCache.object(key))
        return member;

    auto dbMember = memberRepo.getMember(guildId, userId);
    auto dbUser = userRepo.getUser(userId);
    if (dbMember.has_value() && dbUser.has_value()) {
        dbMember->user = dbUser.value();
        auto *member = new Discord::Member(dbMember.value());
        memberCache.insert(key, member);
        return member;
    }

    return nullptr;
}

QString UserManager::getDisplayName(Snowflake userId, std::optional<Snowflake> guildId)
{
    Discord::User *user = getUser(userId);
    if (!user)
        return QString(tr("Unknown User"));

    if (guildId.has_value()) {
        Discord::Member *member = getMember(guildId.value(), userId);
        if (member && member->nick.hasValue() && !member->nick.isNull())
            return member->nick;
    }

    return user->getDisplayName();
}

void UserManager::saveUser(const Discord::User &user)
{
    userCache.insert(user.id, new Discord::User(user));
    userRepo.saveUser(user);
}

void UserManager::saveUsers(const QList<Discord::User> &users)
{
    for (const auto &user : users)
        userCache.insert(user.id, new Discord::User(user));

    userRepo.saveUsers(users);
}

void UserManager::saveMember(Snowflake guildId, Snowflake userId, const Discord::Member &member)
{
    MemberKey key{ guildId, userId };

    memberCache.insert(key, new Discord::Member(member));
    memberRepo.saveMember(guildId, userId, member);
}

void UserManager::saveMemberWithUser(Snowflake guildId, const Discord::Member &member)
{
    if (member.user.hasValue()) {
        saveUser(member.user.get());
        saveMember(guildId, member.user->id, member);
    }
}

} // namespace Core
} // namespace Acheron
