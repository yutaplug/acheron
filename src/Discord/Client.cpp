#include "Client.hpp"

#include <QDebug>
#include <QJsonObject>

#include "Enums.hpp"
#include "Core/Logging.hpp"
#include "Proto/ProtoReader.hpp"
#include "Proto/UserSettings.hpp"

namespace Acheron {
namespace Discord {

Client::Client(const QString &token, const QString &gatewayUrl, const QString &baseUrl,
               CaptchaResolver *captchaResolver, QObject *parent)
    : QObject(parent), token(token), baseUrl(baseUrl)
{
    identity.regenerateClientHeartbeatSessionId();

    gateway = new Gateway(token, gatewayUrl, identity, this);
    httpClient = new HttpClient(baseUrl, token, identity, captchaResolver, this);

    connect(gateway, &Gateway::connected, this, &Client::onConnected);
    connect(gateway, &Gateway::disconnected, this, &Client::onDisconnected);

    connect(gateway, &Gateway::gatewayReady, this, &Client::onGatewayReady);
    connect(gateway, &Gateway::gatewayReadySupplemental, this, &Client::onGatewayReadySupplemental);
    connect(gateway, &Gateway::gatewayMessageCreate, this, &Client::onGatewayMessageCreate);
    connect(gateway, &Gateway::gatewayMessageUpdate, this, &Client::onGatewayMessageUpdate);
    connect(gateway, &Gateway::gatewayMessageDelete, this, &Client::onGatewayMessageDelete);
    connect(gateway, &Gateway::gatewayTypingStart, this, &Client::typingStart);
    connect(gateway, &Gateway::gatewayChannelCreate, this, &Client::onGatewayChannelCreate);
    connect(gateway, &Gateway::gatewayChannelUpdate, this, &Client::onGatewayChannelUpdate);
    connect(gateway, &Gateway::gatewayChannelDelete, this, &Client::onGatewayChannelDelete);
    connect(gateway, &Gateway::gatewayGuildMembersChunk, this, &Client::guildMembersChunk);
    connect(gateway, &Gateway::gatewayGuildMemberUpdate, this, &Client::guildMemberUpdated);
    connect(gateway, &Gateway::gatewayGuildRoleCreate, this, &Client::onGatewayGuildRoleCreate);
    connect(gateway, &Gateway::gatewayGuildRoleUpdate, this, &Client::onGatewayGuildRoleUpdate);
    connect(gateway, &Gateway::gatewayGuildRoleDelete, this, &Client::onGatewayGuildRoleDelete);
    connect(gateway, &Gateway::gatewayMessageAck, this, &Client::messageAcked);
    connect(gateway, &Gateway::gatewayMessageReactionAdd, this, &Client::messageReactionAdd);
    connect(gateway, &Gateway::gatewayMessageReactionAddMany, this, &Client::messageReactionAddMany);
    connect(gateway, &Gateway::gatewayMessageReactionRemove, this, &Client::messageReactionRemove);
    connect(gateway, &Gateway::gatewayMessageReactionRemoveAll, this, &Client::messageReactionRemoveAll);
    connect(gateway, &Gateway::gatewayMessageReactionRemoveEmoji, this, &Client::messageReactionRemoveEmoji);
    connect(gateway, &Gateway::gatewayUserGuildSettingsUpdate, this,
            &Client::userGuildSettingsUpdated);
    connect(gateway, &Gateway::gatewayGuildMemberListUpdate, this, &Client::guildMemberListUpdate);
    connect(gateway, &Gateway::gatewayVoiceStateUpdate, this, &Client::voiceStateUpdated);
    connect(gateway, &Gateway::gatewayVoiceServerUpdate, this, &Client::voiceServerUpdated);
    connect(gateway, &Gateway::gatewayRelationshipAdd, this, &Client::relationshipAdded);
    connect(gateway, &Gateway::gatewayRelationshipUpdate, this, &Client::relationshipUpdated);
    connect(gateway, &Gateway::gatewayRelationshipRemove, this, &Client::relationshipRemoved);
    connect(gateway, &Gateway::gatewayUserNoteUpdate, this, &Client::userNoteUpdated);
    connect(gateway, &Gateway::reconnecting, this, [this](int attempt, int maxAttempts) {
        setState(Core::ConnectionState::Connecting);
        emit reconnecting(attempt, maxAttempts);
    });
}

Client::~Client()
{
    // join and delete the http clients thread before everything else
    delete httpClient;
    httpClient = nullptr;
}

void Client::start()
{
    setState(Core::ConnectionState::Connecting);
    gateway->start();
}

void Client::stop()
{
    setState(Core::ConnectionState::Disconnecting);
    gateway->stop();
}

[[nodiscard]] Core::ConnectionState Client::getState() const
{
    return state;
}

void Client::fetchLatestMessages(Snowflake channelId, int limit, MessagesCallback callback)
{
    QString endpoint = "/channels/" + QString::number(channelId) + "/messages";
    QUrlQuery query;
    query.addQueryItem("limit", QString::number(limit));

    httpClient->get(endpoint, query, [this, channelId, callback](const HttpResponse &response) {
        if (!response.success) {
            qCWarning(LogDiscord) << "Failed to fetch latest messages: " << response.error;
            callback({ {}, "Failed to fetch latest messages: " + response.error });
            return;
        }

        QList<Message> results;
        QJsonArray arr = QJsonDocument::fromJson(response.body).array();
        for (const QJsonValue &val : arr)
            results.append(Message::fromJson(val.toObject()));

        callback({ results });
    });
}

void Client::fetchHistory(Snowflake channelId, Snowflake beforeId, int limit,
                          MessagesCallback callback)
{
    QString endpoint = "/channels/" + QString::number(channelId) + "/messages";
    QUrlQuery query;
    query.addQueryItem("before", QString::number(beforeId));
    query.addQueryItem("limit", QString::number(limit));

    httpClient->get(endpoint, query, [this, channelId, callback](const HttpResponse &response) {
        if (!response.success) {
            qCWarning(LogDiscord) << "Failed to fetch history: " << response.error;
            callback({ {}, "Failed to fetch history: " + response.error });
            return;
        }

        QList<Message> results;
        QJsonArray arr = QJsonDocument::fromJson(response.body).array();
        for (const QJsonValue &val : arr)
            results.append(Message::fromJson(val.toObject()));

        callback({ results });
    });
}

void Client::fetchUserProfile(Snowflake userId, Snowflake guildId, ProfileCallback callback)
{
    QString endpoint = "/users/" + QString::number(userId) + "/profile";
    QUrlQuery query;
    query.addQueryItem("type", "popout");
    query.addQueryItem("with_mutual_guilds", "true");
    query.addQueryItem("with_mutual_friends", "true");
    query.addQueryItem("with_mutual_friends_count", "false");
    if (guildId.isValid())
        query.addQueryItem("guild_id", QString::number(guildId));

    httpClient->get(endpoint, query, [userId, callback](const HttpResponse &response) {
        if (!response.success) {
            qCWarning(LogDiscord) << "Failed to fetch user profile for" << userId << ":"
                                  << response.error;
            callback({ {}, "Failed to fetch user profile: " + response.error });
            return;
        }

        UserProfile profile = UserProfile::fromJson(QJsonDocument::fromJson(response.body).object());
        callback({ profile });
    });
}

void Client::setUserNote(Snowflake userId, const QString &note)
{
    QString endpoint = "/users/@me/notes/" + QString::number(userId);
    QJsonObject payload;
    payload["note"] = note;

    httpClient->put(endpoint, payload, [userId](const HttpResponse &response) {
        if (!response.success)
            qCWarning(LogDiscord) << "Failed to set note for user" << userId << ":"
                                  << response.error;
    });
}

void Client::onConnected()
{
    qInfo() << "Connected to gateway";

    setState(Core::ConnectionState::Connected);
}

void Client::onDisconnected(CloseCode code, const QString &reason)
{
    qWarning() << "Disconnected from gateway: " << code << reason;

    // Fatal close codes — no reconnection, transition straight to Disconnected
    if (code == CloseCode::AUTHENTICATION_FAILED ||
        code == CloseCode::INVALID_SHARD ||
        code == CloseCode::SHARDING_REQUIRED ||
        code == CloseCode::INVALID_API_VERSION ||
        code == CloseCode::INVALID_INTENTS ||
        code == CloseCode::DISALLOWED_INTENTS) {
        setState(Core::ConnectionState::Disconnected);
        if (code == CloseCode::AUTHENTICATION_FAILED) {
            emit errorOccurred("Invalid token");
            emit authenticationFailed();
        } else {
            emit errorOccurred("Fatal gateway error: " + reason);
        }
        return;
    }

    // User-initiated disconnect (via stop()) — transition to Disconnected
    if (state == Core::ConnectionState::Disconnecting) {
        setState(Core::ConnectionState::Disconnected);
        return;
    }

    // Non-fatal: Gateway will handle reconnection automatically
    // stateChanged(Connecting) is emitted via the reconnecting signal
}

void Client::onGatewayReady(const Ready &data)
{
    for (const auto &guild : data.guilds.get()) {
        for (const auto &channel : guild.channels.get()) {
            channelToGuild.insert(channel.id, guild.properties->id.get());
        }
        guildPremiumTiers.insert(guild.properties->id.get(),
                                 guild.properties->premiumTier.hasValue()
                                         ? guild.properties->premiumTier.get()
                                         : PremiumTier::NONE);
    }

    const QByteArray binary = QByteArray::fromBase64(data.userSettingsProto->toUtf8());
    Proto::ProtoReader reader(binary);
    settings = Proto::PreloadedUserSettings::fromProto(reader);
    me = data.user;

    emit ready(data);
}

void Client::onGatewayReadySupplemental(const ReadySupplemental &data)
{
    emit readySupplemental(data);
}

void Client::onGatewayMessageCreate(const Message &msg)
{
    emit messageCreated(msg);
}

void Client::onGatewayMessageUpdate(const Message &msg)
{
    emit messageUpdated(msg);
}

void Client::onGatewayMessageDelete(const MessageDelete &event)
{
    emit messageDeleted(event);
}

void Client::onGatewayChannelCreate(const ChannelCreate &event)
{
    emit channelCreated(event);
}

void Client::onGatewayChannelUpdate(const ChannelUpdate &event)
{
    emit channelUpdated(event);
}

void Client::onGatewayChannelDelete(const ChannelDelete &event)
{
    emit channelDeleted(event);
}

void Client::onGatewayGuildRoleCreate(const GuildRoleCreate &event)
{
    emit guildRoleCreated(event);
}

void Client::onGatewayGuildRoleUpdate(const GuildRoleUpdate &event)
{
    emit guildRoleUpdated(event);
}

void Client::onGatewayGuildRoleDelete(const GuildRoleDelete &event)
{
    emit guildRoleDeleted(event);
}

void Client::sendMessage(Snowflake channelId, const QString &content, const QString &nonce,
                         Snowflake replyToMessageId, const QList<Core::PendingAttachment> &attachments)
{
    // todo extract to struct probably
    QJsonObject payload;
    payload["content"] = content;
    payload["flags"] = 0;
    payload["mobile_network_type"] = "unknown";
    payload["nonce"] = nonce;
    payload["tts"] = false;

    if (replyToMessageId.isValid()) {
        QJsonObject messageReference;
        messageReference["message_id"] = QString::number(replyToMessageId);
        messageReference["channel_id"] = QString::number(channelId);
        payload["message_reference"] = messageReference;
    }

    if (!attachments.isEmpty()) {
        auto state = std::make_shared<UploadState>();
        state->channelId = channelId;
        state->payload = payload;
        state->nonce = nonce;
        state->attachments = attachments;
        for (int i = 0; i < attachments.size(); i++) {
            state->uploadFilenames.append(QString());
            state->uploaded.append(false);
        }
        state->cancelFlag = std::make_shared<std::atomic<bool>>(false);
        activeUploads.insert(nonce, state);
        uploadAttachmentsAndSend(state);
        return;
    }

    QString endpoint = "/channels/" + QString::number(channelId) + "/messages";
    httpClient->post(endpoint, payload, [this, channelId, nonce](const HttpResponse &response) {
        if (!response.success) {
            qCWarning(LogDiscord) << "Failed to send message:" << response.error
                                  << "Status:" << response.statusCode;
            emit messageSendFailed(nonce, response.error);
            return;
        }

        qCInfo(LogDiscord) << "Message sent successfully to channel" << channelId;
    });
}

void Client::uploadAttachmentsAndSend(const std::shared_ptr<UploadState> &state)
{
    QJsonArray files;
    for (int i = 0; i < state->attachments.size(); i++) {
        QJsonObject file;
        file["id"] = QString::number(i);
        file["filename"] = state->attachments[i].filename;
        file["file_size"] = state->attachments[i].size;
        files.append(file);
    }
    QJsonObject body;
    body["files"] = files;

    QString endpoint = "/channels/" + QString::number(state->channelId) + "/attachments";
    httpClient->post(endpoint, body, [this, state](const HttpResponse &response) {
        if (state->cancelFlag->load()) {
            settleUpload(state);
            return;
        }
        if (!response.success) {
            qCWarning(LogDiscord) << "Failed to request upload slots:" << response.error
                                  << "Status:" << response.statusCode;
            settleUpload(state);
            emit messageSendFailed(state->nonce, response.error);
            return;
        }

        const auto uploadSlots = QJsonDocument::fromJson(response.body).object()["attachments"].toArray();
        if (uploadSlots.size() != state->attachments.size()) {
            settleUpload(state);
            emit messageSendFailed(state->nonce, "Unexpected upload slot response");
            return;
        }

        QStringList uploadUrls;
        for (int i = 0; i < state->attachments.size(); i++)
            uploadUrls.append(QString());
        for (const QJsonValue &slotValue : uploadSlots) {
            auto slot = slotValue.toObject();
            int index = slot["id"].toVariant().toInt();
            if (index < 0 || index >= state->attachments.size()) {
                // wut
                settleUpload(state);
                emit messageSendFailed(state->nonce, "Unexpected upload slot response");
                return;
            }
            state->uploadFilenames[index] = slot["upload_filename"].toString();
            uploadUrls[index] = slot["upload_url"].toString();
        }

        state->remaining = state->attachments.size();
        for (int index = 0; index < state->attachments.size(); index++) {
            const auto &attachment = state->attachments[index];

            auto onDone = [this, state, index](const HttpResponse &putResponse) {
                state->remaining--;
                if (putResponse.success) {
                    state->uploaded[index] = true;
                } else if (!state->failed && !state->cancelFlag->load()) {
                    state->failed = true;
                    qCWarning(LogDiscord) << "Attachment upload failed:" << putResponse.error
                                          << "Status:" << putResponse.statusCode;
                    emit messageSendFailed(state->nonce, putResponse.error);
                    state->cancelFlag->store(true); // abort !!!
                }
                if (state->remaining > 0)
                    return;

                if (state->failed || state->cancelFlag->load()) {
                    cleanupUploadedSlots(state);
                    settleUpload(state);
                    return;
                }
                finishUpload(state);
            };
            auto onProgress = [this, state, index](qint64 sent, qint64 total) {
                emit attachmentUploadProgress(state->nonce, index, sent, total);
            };

            // pasted bitmap from mem, otherwise from disk
            if (!attachment.data.isEmpty())
                httpClient->putExternal(uploadUrls[index], attachment.data, attachment.mimeType,
                                        onDone, onProgress, state->cancelFlag);
            else
                httpClient->putExternalFile(uploadUrls[index], attachment.filePath,
                                            attachment.mimeType, onDone, onProgress,
                                            state->cancelFlag);
        }
    });
}

void Client::finishUpload(const std::shared_ptr<UploadState> &state)
{
    if (state->cancelFlag->load()) {
        cleanupUploadedSlots(state);
        settleUpload(state);
        return;
    }

    QJsonArray attachmentsJson;
    for (int i = 0; i < state->attachments.size(); i++) {
        const auto &attachment = state->attachments[i];
        QJsonObject obj;
        obj["id"] = QString::number(i);
        obj["filename"] = attachment.filename;
        obj["uploaded_filename"] = state->uploadFilenames[i];
        if (attachment.isSpoiler)
            obj["is_spoiler"] = true;
        if (!attachment.description.isEmpty())
            obj["description"] = attachment.description;
        attachmentsJson.append(obj);
    }
    QJsonObject payload = state->payload;
    payload["attachments"] = attachmentsJson;

    settleUpload(state);

    QString endpoint = "/channels/" + QString::number(state->channelId) + "/messages";
    httpClient->post(endpoint, payload, [this, state](const HttpResponse &response) {
        if (!response.success) {
            qCWarning(LogDiscord) << "Failed to send message:" << response.error
                                  << "Status:" << response.statusCode;
            emit messageSendFailed(state->nonce, response.error);
            return;
        }
    });
}

void Client::cleanupUploadedSlots(const std::shared_ptr<UploadState> &state)
{
    for (int i = 0; i < state->uploaded.size(); i++) {
        if (!state->uploaded[i] || state->uploadFilenames[i].isEmpty())
            continue;
        httpClient->delete_("/attachments/" + state->uploadFilenames[i], [](const auto &) { });
    }
}

void Client::settleUpload(const std::shared_ptr<UploadState> &state)
{
    activeUploads.remove(state->nonce);
}

bool Client::cancelMessageSend(const QString &nonce)
{
    auto it = activeUploads.constFind(nonce);
    if (it == activeUploads.constEnd())
        return false;
    it.value()->cancelFlag->store(true);
    return true;
}

void Client::editMessage(Snowflake channelId, Snowflake messageId, const QString &content)
{
    QString endpoint = "/channels/" + QString::number(channelId) + "/messages/" +
                       QString::number(messageId);

    QJsonObject payload;
    payload["content"] = content;

    httpClient->patch(endpoint, payload, [this, channelId, messageId](const HttpResponse &response) {
        if (!response.success)
            qCWarning(LogDiscord) << "Failed to edit message" << messageId << "in channel"
                                  << channelId << ":" << response.error;
        else
            qCInfo(LogDiscord) << "Message" << messageId << "edited in channel" << channelId;
    });
}

void Client::deleteMessage(Snowflake channelId, Snowflake messageId)
{
    QString endpoint = "/channels/" + QString::number(channelId) + "/messages/" +
                       QString::number(messageId);

    httpClient->delete_(endpoint, [this, channelId, messageId](const HttpResponse &response) {
        if (!response.success)
            qCWarning(LogDiscord) << "Failed to delete message" << messageId << "in channel"
                                  << channelId << ":" << response.error;
        else
            qCInfo(LogDiscord) << "Message" << messageId << "deleted from channel" << channelId;
    });
}

void Client::pinMessage(Snowflake channelId, Snowflake messageId)
{
    QString endpoint = "/channels/" + QString::number(channelId) + "/pins/" +
                       QString::number(messageId);

    httpClient->put(endpoint, QJsonObject{}, [this, channelId, messageId](const HttpResponse &response) {
        if (!response.success)
            qCWarning(LogDiscord) << "Failed to pin message" << messageId << "in channel"
                                  << channelId << ":" << response.error;
        else
            qCInfo(LogDiscord) << "Message" << messageId << "pinned in channel" << channelId;
    });
}

void Client::unpinMessage(Snowflake channelId, Snowflake messageId)
{
    QString endpoint = "/channels/" + QString::number(channelId) + "/pins/" +
                       QString::number(messageId);

    httpClient->delete_(endpoint, [this, channelId, messageId](const HttpResponse &response) {
        if (!response.success)
            qCWarning(LogDiscord) << "Failed to unpin message" << messageId << "in channel"
                                  << channelId << ":" << response.error;
        else
            qCInfo(LogDiscord) << "Message" << messageId << "unpinned from channel" << channelId;
    });
}

void Client::addReaction(Snowflake channelId, Snowflake messageId, const QString &emoji,
                         bool isBurst)
{
    QString encoded = QUrl::toPercentEncoding(emoji, ":");
    int type = isBurst ? 1 : 0;
    QString endpoint = "/channels/" + QString::number(channelId) + "/messages/" +
                       QString::number(messageId) + "/reactions/" + encoded +
                       "/%40me?location=Message%20Inline%20Button&type=" + QString::number(type);

    httpClient->put(endpoint, QJsonObject{}, [this, channelId, messageId](const HttpResponse &response) {
        if (!response.success)
            qCWarning(LogDiscord) << "Failed to add reaction on message" << messageId
                                  << "in channel" << channelId << ":" << response.error;
    });
}

void Client::removeReaction(Snowflake channelId, Snowflake messageId, const QString &emoji,
                            bool isBurst)
{
    QString encoded = QUrl::toPercentEncoding(emoji, ":");
    QString endpoint = "/channels/" + QString::number(channelId) + "/messages/" +
                       QString::number(messageId) + "/reactions/" + encoded + "/%40me";
    if (isBurst)
        endpoint += "?burst=true";
    else
        endpoint += "?type=0";

    httpClient->delete_(endpoint, [this, channelId, messageId](const HttpResponse &response) {
        if (!response.success)
            qCWarning(LogDiscord) << "Failed to remove reaction on message" << messageId
                                  << "in channel" << channelId << ":" << response.error;
    });
}

void Client::debugForceReconnect()
{
    gateway->debugForceReconnect();
}

void Client::ackMessage(Snowflake channelId, Snowflake messageId, int flags, int lastViewed)
{
    QString endpoint = "/channels/" + QString::number(channelId) + "/messages/" +
                       QString::number(messageId) + "/ack";

    QJsonObject payload;
    payload["flags"] = flags;
    payload["last_viewed"] = lastViewed;
    payload["token"] = QJsonValue::Null;

    httpClient->post(endpoint, payload, [this, channelId](const HttpResponse &response) {
        if (!response.success)
            qCWarning(LogDiscord) << "Failed to ack message in channel" << channelId
                                  << ":" << response.error;
    });
}

void Client::ackBulk(const QList<AckEntry> &entries)
{
    QJsonArray readStates;
    for (const auto &entry : entries) {
        QJsonObject obj;
        obj["channel_id"] = QString::number(entry.channelId);
        obj["message_id"] = QString::number(entry.messageId);
        obj["read_state_type"] = entry.readStateType;
        readStates.append(obj);
    }

    QJsonObject payload;
    payload["read_states"] = readStates;
    httpClient->post("/read-states/ack-bulk", payload, [this](const HttpResponse &response) {
        if (!response.success)
            qCWarning(LogDiscord) << "Failed to bulk ack:" << response.error;
    });
}

void Client::subscribeToGuildChannel(Snowflake guildId, Snowflake channelId,
                                     const QList<QPair<int, int>> &ranges)
{
    gateway->subscribeToGuild(guildId, channelId, ranges);
    subscribedGuilds.insert(guildId);
}

void Client::ensureSubscriptionByChannel(Snowflake channelId)
{
    if (!channelToGuild.contains(channelId))
        return;

    Snowflake guildId = channelToGuild.value(channelId);
    if (!subscribedGuilds.contains(guildId)) {
        QList<QPair<int, int>> defaultRanges = { { 0, 99 } };
        subscribeToGuildChannel(guildId, channelId, defaultRanges);
    }
}

Snowflake Client::getGuildIdForChannel(Snowflake channelId) const
{
    return channelToGuild.value(channelId, Snowflake::Invalid);
}

PremiumTier Client::getGuildPremiumTier(Snowflake guildId) const
{
    return guildPremiumTiers.value(guildId, PremiumTier::NONE);
}

qint64 Client::getMaxUploadSize(Snowflake channelId) const
{
    constexpr qint64 MiB = 1024 * 1024;

    auto premiumType = me.premiumType.hasValue() ? me.premiumType.get() : PremiumType::NONE;
    qint64 userLimit = 10 * MiB;
    switch (premiumType) {
    case PremiumType::TIER_1:
        userLimit = 10 * MiB;
        break;
    case PremiumType::TIER_2:
        userLimit = 500 * MiB;
        break;
    case PremiumType::TIER_3:
        userLimit = 50 * MiB;
        break;
    default:
        break;
    }

    qint64 guildLimit = 10 * MiB;
    Snowflake guildId = getGuildIdForChannel(channelId);
    if (guildId.isValid()) {
        switch (getGuildPremiumTier(guildId)) {
        case PremiumTier::TIER_2:
            guildLimit = 50 * MiB;
            break;
        case PremiumTier::TIER_3:
            guildLimit = 100 * MiB;
            break;
        default:
            break;
        }
    }

    return qMax(userLimit, guildLimit);
}

void Client::sendVoiceStateUpdate(Snowflake guildId, Snowflake channelId, bool selfMute, bool selfDeaf)
{
    gateway->sendVoiceStateUpdate(guildId, channelId, selfMute, selfDeaf);
}

void Client::requestGuildMembers(Snowflake guildId, const QList<Snowflake> &userIds)
{
    gateway->requestGuildMembers(guildId, userIds);
}

[[nodiscard]] const Proto::PreloadedUserSettings &Client::getSettings() const
{
    return settings;
}

[[nodiscard]] const User &Client::getMe() const
{
    return me;
}

void Client::setState(Core::ConnectionState state)
{
    if (this->state != state) {
        this->state = state;
        emit stateChanged(state);
    }
}

} // namespace Discord
} // namespace Acheron