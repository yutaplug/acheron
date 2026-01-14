#include "ChannelFilterProxyModel.hpp"
#include "ChannelTreeModel.hpp"
#include "ChannelNode.hpp"

#include "Core/ClientInstance.hpp"
#include "Core/PermissionManager.hpp"
#include "Discord/Enums.hpp"

namespace Acheron {
namespace UI {

ChannelFilterProxyModel::ChannelFilterProxyModel(Core::Session *session, QObject *parent)
    : QSortFilterProxyModel(parent), session(session), channelModel(nullptr)
{
}

void ChannelFilterProxyModel::setSourceModel(QAbstractItemModel *sourceModel)
{
    channelModel = qobject_cast<ChannelTreeModel *>(sourceModel);
    QSortFilterProxyModel::setSourceModel(sourceModel);
}

void ChannelFilterProxyModel::invalidateFilter()
{
    QSortFilterProxyModel::invalidateFilter();
}

bool ChannelFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!channelModel)
        return true;

    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
        return true;

    auto nodeType = static_cast<ChannelNode::Type>(index.data(Qt::UserRole + 2).toInt());

    if (nodeType == ChannelNode::Type::Channel) {
        Core::Snowflake userId = getUserIdForNode(index);
        if (!userId.isValid())
            return true;

        auto *instance = session->client(userId);
        if (!instance)
            return true;

        auto *permissionManager = instance->permissions();
        if (!permissionManager)
            return true;

        Core::Snowflake channelId = Core::Snowflake(index.data(Qt::UserRole).toULongLong());
        return permissionManager->hasChannelPermission(userId, channelId,
                                                       Discord::Permission::VIEW_CHANNEL);
    }

    if (nodeType == ChannelNode::Type::Category)
        return hasVisibleChildren(index);

    return true;
}

bool ChannelFilterProxyModel::hasVisibleChildren(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return false;

    int rowCount = sourceModel()->rowCount(parent);
    for (int i = 0; i < rowCount; ++i) {
        if (filterAcceptsRow(i, parent))
            return true;
    }

    return false;
}

// todo: uhhh
Core::Snowflake ChannelFilterProxyModel::getUserIdForNode(const QModelIndex &index) const
{
    if (!channelModel)
        return Core::Snowflake::Invalid;

    QModelIndex current = index;
    while (current.isValid()) {
        auto nodeType = static_cast<ChannelNode::Type>(current.data(Qt::UserRole + 2).toInt());
        if (nodeType == ChannelNode::Type::Account)
            return Core::Snowflake(current.data(Qt::UserRole).toULongLong());
        current = current.parent();
    }

    return Core::Snowflake::Invalid;
}

} // namespace UI
} // namespace Acheron
