#include "MemberListModel.hpp"

#include "Discord/CdnUrls.hpp"
#include "Core/UserManager.hpp"

namespace Acheron {
namespace UI {

constexpr static QSize AvatarRequestSize = QSize(32, 32);

MemberListModel::MemberListModel(Core::ImageManager *imageManager, QObject *parent)
    : QAbstractListModel(parent), imageManager(imageManager)
{
    connect(imageManager, &Core::ImageManager::imageFetched, this, &MemberListModel::onImageFetched);
}

void MemberListModel::setManager(Core::MemberListManager *newManager)
{
    beginResetModel();
    disconnectManager();
    manager = newManager;
    avatarTracker.clear();
    connectManager();
    endResetModel();
}

int MemberListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !manager)
        return 0;

    return manager->totalItemCount();
}

QVariant MemberListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !manager)
        return {};

    int row = index.row();
    if (row < 0 || row >= manager->totalItemCount())
        return {};

    const auto *item = manager->itemAt(row);

    if (!item) {
        switch (role) {
        case ItemTypeRole:
            return static_cast<int>(Core::MemberListItem::Type::Placeholder);
        case LoadedRole:
            return false;
        default:
            return {};
        }
    }

    switch (role) {
    case ItemTypeRole:
        return static_cast<int>(item->type);
    case LoadedRole:
        return true;
    case UserIdRole:
        return item->type == Core::MemberListItem::Type::Member
                       ? QVariant::fromValue(static_cast<quint64>(item->userId))
                       : QVariant();
    case UsernameRole:
        if (item->type != Core::MemberListItem::Type::Member)
            return QString();
        if (!item->displayName.isEmpty())
            return item->displayName;
        return userManager && item->userId.isValid()
                       ? userManager->getDisplayName(item->userId, manager->currentGuildId())
                       : QString();
    case ActivityRole:
        return item->type == Core::MemberListItem::Type::Member && userManager
                       ? userManager->getActivityText(item->userId)
                       : QString();
    case ActivityEmojiRole: {
        if (item->type != Core::MemberListItem::Type::Member || !userManager ||
            !item->userId.isValid())
            return QVariant();

        auto activity = userManager->getActivity(item->userId);
        if (!activity || !activity->emoji.hasValue() || !activity->emoji->id.hasValue() ||
            !activity->emoji->id->isValid())
            return QVariant();

        const QUrl url = Discord::Cdn::emoji(activity->emoji->id.get(), 32);
        const QSize size(16, 16);
        QPixmap pixmap = imageManager->get(url, size, accountId);
        if (!imageManager->isCached(url, size)) {
            avatarTracker.track(url, index);
            return QVariant();
        }
        return pixmap;
    }
    case ActivityEmojiNameRole: {
        if (item->type != Core::MemberListItem::Type::Member || !userManager ||
            !item->userId.isValid())
            return QString();

        auto activity = userManager->getActivity(item->userId);
        if (!activity || !activity->emoji.hasValue() || !activity->emoji->id.hasValue() ||
            !activity->emoji->id->isValid())
            return QString();
        return activity->emoji->name.get();
    }
    case AvatarRole: {
        if (item->type != Core::MemberListItem::Type::Member)
            return QVariant();

        Core::Snowflake userId = item->userId;
        QString avatarHash;
        if (item->member.user.hasValue() && item->member.user->avatar.hasValue())
            avatarHash = item->member.user->avatar.get();
        else if (userManager) {
            auto user = userManager->getUser(userId);
            if (user && user->avatar.hasValue())
                avatarHash = user->avatar.get();
        }

        QUrl url = Discord::Cdn::userAvatar(userId, avatarHash, AvatarRequestSize.width());
        if (url.isEmpty())
            return QVariant();

        if (imageManager->isCached(url, AvatarRequestSize))
            return imageManager->get(url, AvatarRequestSize, accountId);

        imageManager->get(url, AvatarRequestSize, accountId);
        avatarTracker.track(url, index);

        return QVariant();
    }
    case RoleColorRole:
        return item->type == Core::MemberListItem::Type::Member
                       ? QVariant::fromValue(item->roleColor)
                       : QVariant();
    case GroupNameRole:
        return item->type == Core::MemberListItem::Type::Group
                       ? item->groupName
                       : QString();
    case GroupCountRole:
        return item->type == Core::MemberListItem::Type::Group
                       ? item->groupCount
                       : 0;
    case GroupColorRole:
        return item->type == Core::MemberListItem::Type::Group
                       ? QVariant::fromValue(item->groupColor)
                       : QVariant();
    }

    return {};
}

void MemberListModel::setAccount(Core::Snowflake id)
{
    accountId = id;
}

void MemberListModel::setUserManager(Core::UserManager *newUserManager)
{
    if (userManager == newUserManager)
        return;

    if (userManager)
        disconnect(userManager, nullptr, this, nullptr);

    userManager = newUserManager;
    if (userManager) {
        connect(userManager, &Core::UserManager::presenceChanged, this,
                &MemberListModel::onPresenceChanged);
    }

    if (manager && manager->totalItemCount() > 0)
        emit dataChanged(index(0, 0), index(manager->totalItemCount() - 1, 0),
                         { ActivityRole, ActivityEmojiRole, ActivityEmojiNameRole,
                           Qt::SizeHintRole });
}

void MemberListModel::onListAboutToReset()
{
    beginResetModel();
}

void MemberListModel::onListReset()
{
    avatarTracker.clear();
    endResetModel();
}

void MemberListModel::onImageFetched(const QUrl &url, const QSize &size, const QPixmap &pixmap)
{
    Q_UNUSED(size);
    Q_UNUSED(pixmap);

    avatarTracker.notify(url, [this](const QModelIndex &index) {
        if (index.isValid())
            emit dataChanged(index, index);
    });
}

void MemberListModel::onPresenceChanged(Core::Snowflake userId)
{
    if (!manager || !userId.isValid())
        return;

    for (int row = 0; row < manager->totalItemCount(); ++row) {
        const auto *item = manager->itemAt(row);
        if (!item || item->type != Core::MemberListItem::Type::Member ||
            item->userId != userId)
            continue;

        const QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx,
                         { ActivityRole, ActivityEmojiRole, ActivityEmojiNameRole,
                           Qt::SizeHintRole });
    }
}

void MemberListModel::connectManager()
{
    if (!manager)
        return;

    connect(manager, &Core::MemberListManager::listAboutToReset,
            this, &MemberListModel::onListAboutToReset);
    connect(manager, &Core::MemberListManager::listReset,
            this, &MemberListModel::onListReset);
}

void MemberListModel::disconnectManager()
{
    if (!manager)
        return;

    disconnect(manager, nullptr, this, nullptr);
}

} // namespace UI
} // namespace Acheron
