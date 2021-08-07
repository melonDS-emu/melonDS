/*
    Copyright 2016-2021 Arisotura, WaluigiWare64

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include "UpdateUtil.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>

#include <QFile>

#include <QString>

#include <QVersionNumber>


QByteArray request(QString& errString, QUrl webURL, QByteArray token="")
{
    QNetworkAccessManager* manager = new QNetworkAccessManager();
    QNetworkRequest request(webURL);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, true);
    if (!token.isEmpty())
        request.setRawHeader("Authorization", QByteArray("token ") + token);
    QNetworkReply* reply = manager->get(request);
    
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (reply->error() != QNetworkReply::NoError)
        errString = reply->errorString();
    else
        errString = "";
    return reply->readAll();
}

namespace Updater
{
    QUrl downloadURL;
    QString downloadName;
    QByteArray accessToken;

#if defined(CI_PLATFORM_GITHUB)
    bool checkForUpdatesDevGitHub(QString& errString, QString& latestVersion, QByteArray token)
    {
        accessToken = token;
        
        QJsonDocument jsonWorkflows = QJsonDocument::fromJson(request(errString,
            "https://api.github.com/repos/" + githubRepo + "/actions/workflows/" + pipelineFile + "/runs?event=push&status=success&per_page=1", accessToken));

        if (!errString.isEmpty())
            return false;

        latestVersion = jsonWorkflows["workflow_runs"][0]["head_commit"]["id"].toString();

        QJsonDocument jsonArtifacts = QJsonDocument::fromJson(request(errString, jsonWorkflows["workflow_runs"][0]["artifacts_url"].toString(), accessToken));
        if (!errString.isEmpty())
            return false;

        downloadURL = jsonArtifacts["artifacts"][0]["archive_download_url"].toString();
        downloadName = "melonDS.zip";

        if (latestVersion != MELONDS_COMMIT)
            return true;
        else
            return false;
    }

#elif defined(CI_PLATFORM_AZURE)
    bool checkForUpdatesDevAzure(QString& errString, QString& latestVersion)
    {
        QJsonDocument pipelineDefinitions = QJsonDocument::fromJson(request(errString, QStringLiteral(
            "https://dev.azure.com/%1/_apis/build/definitions?api-version=6.0&name=%2").arg(azureProject).arg(pipelineName)));
        if (!errString.isEmpty())
            return false;

        QJsonDocument pipelineInfo = QJsonDocument::fromJson(request(errString, QStringLiteral(
            "https://dev.azure.com/%1/_apis/build/builds?api-version=6.0&reasonFilter=individualCI&resultFilter=succeeded&$top=1&definitions=%2")
            .arg(azureProject).arg(pipelineDefinitions["value"][0]["id"].toInt())));
        if (!errString.isEmpty())
            return false;
        latestVersion = pipelineInfo["value"][0]["sourceVersion"].toString();

        QJsonDocument artifactInfo = QJsonDocument::fromJson(request(errString, QStringLiteral(
            "https://dev.azure.com/%1/_apis/build/builds/%2/artifacts?api-version=6.0").arg(azureProject).arg(pipelineInfo["value"][0]["id"].toInt())));
        if (!errString.isEmpty())
            return false;

        downloadURL = artifactInfo["value"][0]["resource"]["downloadUrl"].toString();
        downloadName = "melonDS.zip";

        if (latestVersion != MELONDS_COMMIT)
            return true;
        else
            return false;
    }
#endif

    bool checkForUpdatesStable(QString& errString, QString& latestVersion)
    {
        QJsonDocument latestStable = QJsonDocument::fromJson(request(errString, "https://api.github.com/repos/" + githubRepo + "/releases/latest"));
        if (!errString.isEmpty())
            return false;
        latestVersion = latestStable["name"].toString();
        QVersionNumber latestVersionNumber = QVersionNumber::fromString(latestVersion);
        QVersionNumber currentVersionNumber = QVersionNumber::fromString(MELONDS_VERSION);
        downloadName = stableName.arg(latestVersion);

        for (const auto& releaseAsset: latestStable["assets"].toArray())
        {
            QJsonObject releaseAssetObj = releaseAsset.toObject();
            if (releaseAssetObj["name"].toString() == downloadName)
            {
                downloadURL = releaseAssetObj["browser_download_url"].toString();
                break;
            }
        }
        if (latestVersionNumber > currentVersionNumber)
            return true;
        else
            return false;

    }



    QString downloadUpdate()
    {
        QFile downloadFile(downloadName.isEmpty() ? downloadURL.fileName() : downloadName);

        if(!downloadFile.open(QIODevice::WriteOnly))
            return "Couldn't open the file. Perhaps you don't have the suitable permissions in that directory.";
        QString err;
        QByteArray artifactData ;
        if (!accessToken.isEmpty())
            artifactData = request(err, downloadURL, accessToken);
        else
            artifactData = request(err, downloadURL);

        if (!err.isEmpty())
            return err;

        downloadFile.write(artifactData);
        downloadFile.close();
        return "";
    }
}
