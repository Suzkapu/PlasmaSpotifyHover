// SPDX-License-Identifier: GPL-2.0-or-later

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

void Backend::saveSpotifyTrack(const QString &title, const QString &artist)
{
    if (m_spotifySaving) {
        return;
    }
    const auto helper = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("plasma-spotify-hover/spotify-library.py"));
    if (helper.isEmpty()) {
        finishSpotifySave(i18n("Install the Spotify library helper first"));
        return;
    }

    m_spotifySaving = true;
    m_spotifyStatus = i18n("Saving…");
    Q_EMIT spotifyStatusChanged();

    auto request = QDBusMessage::createMethodCall(QStringLiteral("org.mpris.MediaPlayer2.spotify"),
                                                  QStringLiteral("/org/mpris/MediaPlayer2"),
                                                  QStringLiteral("org.freedesktop.DBus.Properties"),
                                                  QStringLiteral("Get"));
    request << QStringLiteral("org.mpris.MediaPlayer2.Player") << QStringLiteral("Metadata");
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, title, artist, helper](QDBusPendingCallWatcher *watcher) {
        const QDBusPendingReply<QDBusVariant> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            finishSpotifySave(i18n("Could not read the current Spotify track"));
            return;
        }

        const auto metadata = qdbus_cast<QVariantMap>(reply.value().variant());
        auto artists = metadata.value(QStringLiteral("xesam:artist")).toStringList();
        if (artists.isEmpty()) {
            artists = metadata.value(QStringLiteral("xesam:albumArtist")).toStringList();
        }
        if (metadata.value(QStringLiteral("xesam:title")).toString() != title || artists.join(QStringLiteral(", ")) != artist) {
            finishSpotifySave(i18n("The track changed. Click again to save the current track."));
            return;
        }

        static const QRegularExpression trackPattern(QStringLiteral(
            "^(?:spotify:track:|/com/spotify/track/|https://open\\.spotify\\.com/track/)([A-Za-z0-9]{22})(?:\\?[^#]*)?$"));
        auto match = trackPattern.match(metadata.value(QStringLiteral("xesam:url")).toString());
        if (!match.hasMatch()) {
            match = trackPattern.match(metadata.value(QStringLiteral("mpris:trackid")).toString());
        }
        if (!match.hasMatch()) {
            finishSpotifySave(i18n("This item has no Spotify track ID"));
            return;
        }

        auto process = new QProcess(this);
        auto timeout = new QTimer(process);
        timeout->setSingleShot(true);
        connect(timeout, &QTimer::timeout, process, &QProcess::kill);
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                finishSpotifySave(i18n("Could not start the Spotify library helper"));
                process->deleteLater();
            }
        });
        connect(process, &QProcess::finished, this, [this, process](int code, QProcess::ExitStatus status) {
            const auto detail = QString::fromUtf8(process->readAllStandardError()).trimmed().left(180);
            if (status == QProcess::NormalExit && code == 0) {
                finishSpotifySave(i18n("Saved to Liked Songs"));
            } else {
                finishSpotifySave(detail.isEmpty() ? i18n("Could not save to Liked Songs") : detail);
            }
            process->deleteLater();
        });
        process->start(QStringLiteral("python3"), {helper, QStringLiteral("save"), QStringLiteral("spotify:track:") + match.captured(1)});
        timeout->start(25000);
    });
}

void Backend::finishSpotifySave(const QString &message)
{
    m_spotifySaving = false;
    m_spotifyStatus = message;
    Q_EMIT spotifyStatusChanged();
    QTimer::singleShot(5000, this, [this, message] {
        if (!m_spotifySaving && m_spotifyStatus == message) {
            m_spotifyStatus.clear();
            Q_EMIT spotifyStatusChanged();
        }
    });
}
