/*
    Soul Link DS — Milestone 2: hardcoded read endpoint
*/

#include "SoulLinkServer.h"
#include "EmuInstance.h"
#include "NDS.h"

#include <QTcpSocket>
#include <cstdio>
#include <cstring>

// ARM9 address to read. 0x02000000 is the start of main RAM — always present
// once a ROM is loaded, and its contents change as the game runs.
// Replace with a game-specific address (e.g., party HP in HG/SS) once confirmed.
static constexpr melonDS::u32 kWatchAddr = 0x02000000;
static constexpr int kWatchLen = 16;

SoulLinkServer::SoulLinkServer(EmuInstance* emu, QObject* parent)
    : QObject(parent)
    , server(new QTcpServer(this))
    , emuInstance(emu)
{
    connect(server, &QTcpServer::newConnection, this, &SoulLinkServer::onNewConnection);
}

SoulLinkServer::~SoulLinkServer()
{
    server->close();
}

void SoulLinkServer::start(quint16 port)
{
    if (!server->listen(QHostAddress::LocalHost, port))
    {
        fprintf(stderr, "[SoulLink] Failed to listen on port %d: %s\n",
                port, server->errorString().toUtf8().constData());
    }
    else
    {
        fprintf(stderr, "[SoulLink] Listening on 127.0.0.1:%d\n", port);
    }
}

void SoulLinkServer::onNewConnection()
{
    QTcpSocket* client = server->nextPendingConnection();
    if (!client) return;

    // deleteLater keeps Qt happy if the client disconnects before we write
    connect(client, &QTcpSocket::disconnected, client, &QTcpSocket::deleteLater);

    melonDS::NDS* nds = emuInstance->getNDS();
    if (!nds || !nds->MainRAM)
    {
        client->write("ERR no ROM loaded\n");
        client->disconnectFromHost();
        return;
    }

    // ARM9 address → offset into MainRAM
    melonDS::u32 offset = kWatchAddr - 0x02000000;

    // Snapshot the bytes. A torn read across a frame boundary is acceptable
    // for this diagnostic read — we're not writing to RAM here.
    melonDS::u8 buf[kWatchLen];
    memcpy(buf, nds->MainRAM + offset, kWatchLen);

    // Format: "BYTES addr=0x02000000 len=16 data=aabbcc...\n"
    QString line = QString("BYTES addr=0x%1 len=%2 data=")
                       .arg(kWatchAddr, 8, 16, QChar('0'))
                       .arg(kWatchLen);
    for (int i = 0; i < kWatchLen; i++)
        line += QString("%1").arg(buf[i], 2, 16, QChar('0'));
    line += "\n";

    client->write(line.toUtf8());
    client->flush();
    client->disconnectFromHost();
}
