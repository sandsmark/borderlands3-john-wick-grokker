#include <QCoreApplication>

#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

QJsonObject parseFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open" << file.fileName() << file.errorString();
        return {};
    }

    const QJsonArray objects = QJsonDocument::fromJson(file.readAll()).array();
    if (objects.isEmpty()) {
        qWarning() << "Expected array, but it is empty";
        return {};
    }

    // not the most effective, but can't be arsed to make sure stuff is always in order
    QString missionName;
    for (const QJsonValue &val : objects) {
        const QJsonObject object = val.toObject();
        if (object[QStringLiteral("export_type")].toString() == QStringLiteral("BlueprintGeneratedClass")) {
            missionName = object["_jwp_object_name"].toString();
            break;
        }
    }
    qDebug() << "Got mission name" << missionName;

    QString title;

    QJsonObject objectiveSets;
    QHash<QString, QString> objToSet;
    QJsonObject objectiveTitles;

    QJsonObject out;
    for (const QJsonValue &val : objects) {
        const QJsonObject object = val.toObject();
        const QString type = object[QStringLiteral("export_type")].toString();
        if (type == missionName) {
            out["Title"] = object["FormattedMissionName"].toObject()["FormatText"].toObject()["string"].toString();

            QJsonArray objectives;
            for (const QJsonValue &val : object["Objectives"].toArray()) {
                objectives.append(val.toObject()["_jwp_export_dst_name"]);
            }
            out["ObjectivesList"] = objectives;
            QJsonArray sets;
            for (const QJsonValue &val : object["ObjectiveSets"].toArray()) {
                sets.append(val.toObject()["_jwp_export_dst_name"]);
            }
            out["ObjectivesSetList"] = sets;
            continue;
        }

        if (type == QStringLiteral("MissionObjectiveSet")) {
            QJsonArray objectives;
            for (const QJsonValue &val : object["Objectives"].toArray()) {
                objectives.append(val.toObject()["_jwp_export_dst_name"]);
            }
            const QString setName = object["_jwp_object_name"].toString();
            objectiveSets[setName] = objectives;
            continue;
        }
        if (type == QStringLiteral("MissionObjective")) {
            const QString name = object["_jwp_object_name"].toString();
            objectiveTitles[name] = object["FormattedProgressMessage"].toObject()["FormatText"].toObject()["string"];
        }
    }
    out["ObjectiveSets"] = objectiveSets;
    out["ObjectiveTitles"] = objectiveTitles;

    return out;
}

QJsonObject descendDir(const QString &path, const QString &topPath)
{
    QJsonObject object;
    for (const QFileInfo &entry : QDir(path).entryInfoList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs)) {
        if (entry.isDir()) {
            QJsonObject subObject = descendDir(entry.absoluteFilePath(), topPath);
            for (const QString &key: subObject.keys()) {
                object[key] = subObject[key];
            }
            continue;
        }
        if (entry.suffix() != "json") {
            continue;
        }
        const QString name = "/Game/Missions/" + entry.absoluteFilePath().remove(topPath).remove(".json"); // I'm lazy, sue me
        object[name] = parseFile(entry.absoluteFilePath());
    }

    return object;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        qWarning() << "please pass path to directory with the converted json stuff";
        return 1;
    }

    const QString path = QFileInfo(argv[1]).absoluteFilePath();

    QFile out("missions.json");
    out.open(QIODevice::WriteOnly);
    out.write(QJsonDocument(descendDir(path, path)).toJson(QJsonDocument::Indented));

    return 0;
}
