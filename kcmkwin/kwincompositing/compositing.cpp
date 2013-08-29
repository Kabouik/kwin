/**************************************************************************
* KWin - the KDE window manager                                          *
* This file is part of the KDE project.                                  *
*                                                                        *
* Copyright (C) 2013 Antonis Tsiapaliokas <kok3rs@gmail.com>             *
*                                                                        *
* This program is free software; you can redistribute it and/or modify   *
* it under the terms of the GNU General Public License as published by   *
* the Free Software Foundation; either version 2 of the License, or      *
* (at your option) any later version.                                    *
*                                                                        *
* This program is distributed in the hope that it will be useful,        *
* but WITHOUT ANY WARRANTY; without even the implied warranty of         *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
* GNU General Public License for more details.                           *
*                                                                        *
* You should have received a copy of the GNU General Public License      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
**************************************************************************/

#include "compositing.h"

#include <KDE/KCModuleProxy>
#include <KConfigGroup>
#include <KDE/KSharedConfig>

#include <QDBusInterface>
#include <QDBusReply>
#include <QHash>
#include <QDebug>

namespace KWin {
namespace Compositing {

Compositing::Compositing(QObject *parent)
    : QObject(parent)
{
}

bool Compositing::OpenGLIsUnsafe()
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Compositing");
    return kwinConfig.readEntry("OpenGLIsUnsafe", true);
}

bool Compositing::OpenGLIsBroken()
{
    QDBusInterface interface(QStringLiteral("org.kde.kwin"), QStringLiteral("/Compositing"));
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Compositing");

    QString oldBackend = kwinConfig.readEntry("Backend", "OpenGL");
    kwinConfig.writeEntry("Backend", "OpenGL");
    kwinConfig.sync();
    QDBusReply<bool> OpenGLIsBrokenReply = interface.call("OpenGLIsBroken");

    if (OpenGLIsBrokenReply.value()) {
        kwinConfig.writeEntry("Backend", oldBackend);
        kwinConfig.sync();
        return true;
    }

    kwinConfig.writeEntry("OpenGLIsUnsafe", false);
    kwinConfig.sync();
    return false;
}

CompositingType::CompositingType(QObject *parent)
    : QAbstractItemModel(parent) {

    generateCompositing();
}


void CompositingType::generateCompositing()
{
    QHash<QString, CompositingType::CompositingTypeIndex> compositingTypes;

    compositingTypes["OpenGL 31"] = CompositingType::OPENGL31_INDEX;
    compositingTypes["OpenGL 20"] = CompositingType::OPENGL20_INDEX;
    compositingTypes["OpenGL 12"] = CompositingType::OPENGL12_INDEX;
    compositingTypes["XRender"] = CompositingType::XRENDER_INDEX;

    CompositingData data;
    beginResetModel();
    auto it = compositingTypes.begin();
    while (it != compositingTypes.end()) {
        data.name = it.key();
        data.type = it.value();
        m_compositingList << data;
        it++;
    }

    qSort(m_compositingList.begin(), m_compositingList.end(), [](const CompositingData &a, const CompositingData &b) {
            return a.type < b.type;
    });
    endResetModel();
}

QHash< int, QByteArray > CompositingType::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[NameRole] = "NameRole";
    return roleNames;
}

QModelIndex CompositingType::index(int row, int column, const QModelIndex &parent) const
{

if (parent.isValid() || column > 0 || column < 0 || row < 0 || row >= m_compositingList.count()) {
        return QModelIndex();
    }

    return createIndex(row, column);
}

QModelIndex CompositingType::parent(const QModelIndex &child) const
{
    Q_UNUSED(child)

    return QModelIndex();
}

int CompositingType::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

int CompositingType::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_compositingList.count();
}

QVariant CompositingType::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    switch (role) {
        case Qt::DisplayRole:
        case NameRole:
            return m_compositingList.at(index.row()).name;
        default:
            return QVariant();
    }
}

int CompositingType::currentOpenGLType()
{
    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Compositing");
    QString backend = kwinConfig.readEntry("Backend", "OpenGL");
    bool glLegacy = kwinConfig.readEntry("GLLegacy", false);
    bool glCore = kwinConfig.readEntry("GLCore", false);
    int currentIndex = OPENGL20_INDEX;

    if (backend == "OpenGL") {
        if (glLegacy) {
            currentIndex = OPENGL12_INDEX;
        } else if (glCore) {
            currentIndex = OPENGL31_INDEX;
        } else {
            currentIndex = OPENGL20_INDEX;
        }
    } else {
        currentIndex = XRENDER_INDEX;
    }

    return currentIndex;
}

void CompositingType::syncConfig(int openGLType)
{
    QString backend;
    bool glLegacy;
    bool glCore;


    switch (openGLType) {
        case OPENGL31_INDEX:
            backend = "OpenGL";
            glLegacy = false;
            glCore = true;
            break;
        case OPENGL20_INDEX:
            backend = "OpenGL";
            glLegacy = false;
            glCore = false;
            break;
        case OPENGL12_INDEX:
            backend = "OpenGL";
            glLegacy = true;
            glCore = false;
            break;
        case XRENDER_INDEX:
            backend = "XRender";
            glLegacy = false;
            glCore = false;
            break;
    }

    KConfigGroup kwinConfig(KSharedConfig::openConfig("kwinrc"), "Compositing");
    kwinConfig.writeEntry("Backend", backend);
    kwinConfig.writeEntry("GLLegacy", glLegacy);
    kwinConfig.writeEntry("GLCore", glCore);
    kwinConfig.sync();
}

}//end namespace Compositing
}//end namespace KWin