#include "UserManager.hpp"

#include "Logging.hpp"

namespace Acheron {
namespace Core {

UserManager::UserManager(Snowflake accountId, QObject *parent)
    : QObject(parent), userRepo(accountId), memberRepo(accountId)
{
}

UserManager::~UserManager() {}

std::optional<Discord::User> UserManager::getUser(Snowflake userId)
{
    if (auto *user = userCache.object(userId))
        return *user;

    auto dbUser = userRepo.getUser(userId);
    if (!dbUser.has_value())
        return std::nullopt;

    userCache.insert(userId, new Discord::User(dbUser.value()));
    return dbUser;
}

std::optional<Discord::Member> UserManager::getMember(Snowflake guildId, Snowflake userId)
{
    MemberKey key{ guildId, userId };

    if (auto *member = memberCache.object(key))
        return *member;

    auto dbMember = memberRepo.getMember(guildId, userId);
    auto dbUser = userRepo.getUser(userId);
    if (!dbMember.has_value() || !dbUser.has_value())
        return std::nullopt;

    dbMember->user = dbUser.value();
    memberCache.insert(key, new Discord::Member(dbMember.value()));
    return dbMember;
}

std::optional<Discord::Presence> UserManager::getPresence(Snowflake userId) const
{
    auto it = presences.constFind(userId);
    if (it == presences.constEnd())
        return std::nullopt;
    return it.value();
}

std::optional<Discord::Activity> UserManager::getActivity(Snowflake userId) const
{
    auto presence = getPresence(userId);
    if (!presence)
        return std::nullopt;

    const Discord::Activity *activity = presence->getActivity();
    return activity ? std::optional<Discord::Activity>(*activity) : std::nullopt;
}

QString UserManager::getActivityText(Snowflake userId) const
{
    auto presence = getPresence(userId);
    return presence ? presence->activityText() : QString();
}

std::optional<QList<Snowflake>> UserManager::getMemberRoles(Snowflake guildId, Snowflake userId)
{
    if (auto *member = memberCache.object(MemberKey{ guildId, userId }))
        return member->roles.hasValue() ? member->roles.get() : QList<Snowflake>{};

    auto dbMember = memberRepo.getMember(guildId, userId);
    if (!dbMember.has_value())
        return std::nullopt;
    return dbMember->roles.hasValue() ? dbMember->roles.get() : QList<Snowflake>{};
}

QString UserManager::getDisplayName(Snowflake userId, Snowflake guildId)
{
    auto user = getUser(userId);
    if (!user)
        return tr("Unknown User");

    if (guildId.isValid()) {
        auto member = getMember(guildId, userId);
        if (member && member->nick.hasValue())
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
    if (member.presence.hasValue())
        savePresence(member.presence.get(), userId);
}

void UserManager::saveMembers(Snowflake guildId, const QList<Discord::Member> &members)
{
    for (const auto &member : members) {
        Snowflake userId = member.userId.hasValue() ? member.userId.get()
                                                    : Snowflake::Invalid;
        if (!userId.isValid() && member.user.hasValue() && member.user->id.hasValue())
            userId = member.user->id.get();
        if (!userId.isValid())
            continue;

        memberCache.insert(MemberKey{ guildId, userId }, new Discord::Member(member));
        if (member.presence.hasValue())
            savePresence(member.presence.get(), userId);
    }
    memberRepo.saveMembers(guildId, members);
}

void UserManager::removeGuildMembers(Snowflake guildId)
{
    const QList<MemberKey> keys = memberCache.keys();
    for (const MemberKey &key : keys)
        if (key.guildId == guildId)
            memberCache.remove(key);
}

void UserManager::saveMemberWithUser(Snowflake guildId, const Discord::Member &member)
{
    if (member.user.hasValue()) {
        saveUser(member.user.get());
        saveMember(guildId, member.user->id, member);
    }
}

void UserManager::savePresence(const Discord::Presence &presence, Snowflake fallbackUserId)
{
    Snowflake userId = presence.userId;
    if (!userId.isValid())
        userId = fallbackUserId;
    if (!userId.isValid())
        return;

    Discord::Presence merged = presence;
    merged.userId = userId;

    auto existing = presences.constFind(userId);
    if (existing != presences.constEnd()) {
        if (!merged.status.hasValue() && existing->status.hasValue())
            merged.status = existing->status;
        if (!merged.activities.hasValue() && existing->activities.hasValue())
            merged.activities = existing->activities;
    }

    presences.insert(userId, merged);
    emit presenceChanged(userId);
}

void UserManager::savePresences(const QList<Discord::Presence> &newPresences)
{
    for (const auto &presence : newPresences)
        savePresence(presence);
}

void UserManager::saveMergedPresences(const Discord::MergedPresences &merged)
{
    if (merged.friends.hasValue())
        savePresences(merged.friends.get());

    if (!merged.guilds.hasValue())
        return;

    for (const auto &guildPresences : merged.guilds.get())
        savePresences(guildPresences);
}

void UserManager::loadNotesFromReady(const QHash<Snowflake, QString> &readyNotes)
{
    notes = readyNotes;
    qCInfo(LogCore) << "Loaded" << notes.size() << "user notes from READY";
}

void UserManager::setCachedNote(Snowflake userId, const QString &note)
{
    if (note.isEmpty())
        notes.remove(userId);
    else
        notes.insert(userId, note);
    emit noteChanged(userId);
}

std::optional<QString> UserManager::getCachedNote(Snowflake userId) const
{
    auto it = notes.constFind(userId);
    if (it == notes.constEnd())
        return std::nullopt;
    return it.value();
}

} // namespace Core
} // namespace Acheron
