// SPDX-License-Identifier: AGPL-3.0-only

#include "backend.h"

#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDBusObjectPath>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

int Backend::requestSpotifyLike(const QString &title, const QString &artist, int desired)
{
    if (m_spotifyBusy || desired < -1 || desired > 1) {
        return 0;
    }
    const int requestId = ++m_spotifyRequest;
    // Keep the last confirmed state visible while a write is pending.
    m_spotifyLiked = desired < 0 ? -1 : 1 - desired;
    const auto helper = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("plasma-spotify-hover/spotify-library.py"));
    if (helper.isEmpty()) {
        finishSpotifyRequest(-1, i18n("Install the Spotify library helper first"));
        return requestId;
    }

    m_spotifyBusy = true;
    m_spotifyStatus.clear();
    Q_EMIT spotifyStatusChanged();

    auto request = QDBusMessage::createMethodCall(QStringLiteral("org.mpris.MediaPlayer2.spotify"),
                                                  QStringLiteral("/org/mpris/MediaPlayer2"),
                                                  QStringLiteral("org.freedesktop.DBus.Properties"),
                                                  QStringLiteral("Get"));
    request << QStringLiteral("org.mpris.MediaPlayer2.Player") << QStringLiteral("Metadata");
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, title, artist, helper, desired](QDBusPendingCallWatcher *watcher) {
        const QDBusPendingReply<QDBusVariant> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            finishSpotifyRequest(-1, i18n("Could not read the current Spotify track"));
            return;
        }

        const auto metadata = qdbus_cast<QVariantMap>(reply.value().variant());
        auto artists = metadata.value(QStringLiteral("xesam:artist")).toStringList();
        if (artists.isEmpty()) {
            artists = metadata.value(QStringLiteral("xesam:albumArtist")).toStringList();
        }
        if (metadata.value(QStringLiteral("xesam:title")).toString() != title || artists.join(QStringLiteral(", ")) != artist) {
            finishSpotifyRequest(-1, i18n("The track changed. Check Liked Songs again."));
            return;
        }

        static const QRegularExpression trackPattern(QStringLiteral(
            "^(?:spotify:track:|/com/spotify/track/|https://open\\.spotify\\.com/track/)([A-Za-z0-9]{22})(?:\\?[^#]*)?$"));
        auto match = trackPattern.match(metadata.value(QStringLiteral("xesam:url")).toString());
        if (!match.hasMatch()) {
            const auto id = metadata.value(QStringLiteral("mpris:trackid"));
            const auto path = id.value<QDBusObjectPath>().path();
            match = trackPattern.match(path.isEmpty() ? id.toString() : path);
        }
        if (!match.hasMatch()) {
            finishSpotifyRequest(-1, i18n("This item has no Spotify track ID"));
            return;
        }

        auto process = new QProcess(this);
        auto timeout = new QTimer(process);
        timeout->setSingleShot(true);
        connect(timeout, &QTimer::timeout, process, &QProcess::kill);
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                finishSpotifyRequest(-1, i18n("Could not start the Spotify library helper"));
                process->deleteLater();
            }
        });
        connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
            const auto detail = QString::fromUtf8(process->readAllStandardError()).trimmed().left(180);
            const auto result = process->readAllStandardOutput().trimmed();
            if (status == QProcess::NormalExit && code == 0 && (result == "0" || result == "1")) {
                finishSpotifyRequest(result == "1" ? 1 : 0);
            } else {
                finishSpotifyRequest(-1, detail.isEmpty() ? i18n("Could not update Spotify Liked Songs") : detail);
            }
            process->deleteLater();
        });
        const auto action = desired < 0 ? QStringLiteral("status") : (desired == 1 ? QStringLiteral("save") : QStringLiteral("remove"));
        process->start(QStringLiteral("python3"), {helper, action, QStringLiteral("spotify:track:") + match.captured(1)});
        timeout->start(20000);
    });
    return requestId;
}

void Backend::finishSpotifyRequest(int liked, const QString &message)
{
    m_spotifyBusy = false;
    m_spotifyLiked = liked;
    m_spotifyStatus = message;
    Q_EMIT spotifyStatusChanged();
}
