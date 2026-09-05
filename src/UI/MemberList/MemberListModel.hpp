#pragma once

#include <QAbstractListModel>
#include <QPointer>

#include "Core/MemberListManager.hpp"
#include "Core/ImageManager.hpp"
#include "UI/AvatarRequestTracker.hpp"

namespace Acheron {
namespace Core {
class UserManager;
}

namespace UI {

class MemberListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        ItemTypeRole = Qt::UserRole + 1,
        UserIdRole,
        UsernameRole,
        AvatarRole,
        RoleColorRole,
        GroupNameRole,
        GroupCountRole,
        GroupColorRole,
        LoadedRole,
        ActivityRole,
        ActivityEmojiRole,
        ActivityEmojiNameRole,
    };

    explicit MemberListModel(Core::ImageManager *imageManager, QObject *parent = nullptr);

    void setManager(Core::MemberListManager *manager);
    void setAccount(Core::Snowflake accountId);
    void setUserManager(Core::UserManager *userManager);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

private:
    void onListAboutToReset();
    void onListReset();
    void onImageFetched(const QUrl &url, const QSize &size, const QPixmap &pixmap);
    void onPresenceChanged(Core::Snowflake userId);

    void connectManager();
    void disconnectManager();

    QPointer<Core::MemberListManager> manager;
    Core::ImageManager *imageManager;
    Core::Snowflake accountId;
    Core::UserManager *userManager = nullptr;

    mutable AvatarRequestTracker<QPersistentModelIndex> avatarTracker;
};

} // namespace UI
} // namespace Acheron
