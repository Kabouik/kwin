/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

// own
#include "dbusinterface.h"
#include "compositingadaptor.h"

// kwin
#include "abstract_client.h"
#include "atoms.h"
#include "composite.h"
#include "debug_console.h"
#include "main.h"
#include "placement.h"
#include "platform.h"
#include "kwinadaptor.h"
#include "scene.h"
#include "workspace.h"
#include "virtualdesktops.h"
#ifdef KWIN_BUILD_ACTIVITIES
#include "activities.h"
#endif

// Qt
#include <QOpenGLContext>
#include <QDBusServiceWatcher>

namespace KWin
{

DBusInterface::DBusInterface(QObject *parent)
    : QObject(parent)
    , m_serviceName(QStringLiteral("org.kde.KWin"))
{
    (void) new KWinAdaptor(this);

    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/KWin"), this);
    const QByteArray dBusSuffix = qgetenv("KWIN_DBUS_SERVICE_SUFFIX");
    if (!dBusSuffix.isNull()) {
        m_serviceName = m_serviceName + QLatin1Char('.') + dBusSuffix;
    }
    if (!dbus.registerService(m_serviceName)) {
        QDBusServiceWatcher *dog = new QDBusServiceWatcher(m_serviceName, dbus, QDBusServiceWatcher::WatchForUnregistration, this);
        connect (dog, SIGNAL(serviceUnregistered(QString)), SLOT(becomeKWinService(QString)));
    } else {
        announceService();
    }
    dbus.connect(QString(), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reloadConfig"),
                 Workspace::self(), SLOT(slotReloadConfig()));
    connect(kwinApp(), &Application::x11ConnectionChanged, this, &DBusInterface::announceService);
}

void DBusInterface::becomeKWinService(const QString &service)
{
    // TODO: this watchdog exists to make really safe that we at some point get the service
    // but it's probably no longer needed since we explicitly unregister the service with the deconstructor
    if (service == m_serviceName && QDBusConnection::sessionBus().registerService(m_serviceName) && sender()) {
        sender()->deleteLater(); // bye doggy :'(
        announceService();
    }
}

DBusInterface::~DBusInterface()
{
    QDBusConnection::sessionBus().unregisterService(m_serviceName);
    // KApplication automatically also grabs org.kde.kwin, so it's often been used externally - ensure to free it as well
    QDBusConnection::sessionBus().unregisterService(QStringLiteral("org.kde.kwin"));
    if (kwinApp()->x11Connection()) {
        xcb_delete_property(kwinApp()->x11Connection(), kwinApp()->x11RootWindow(), atoms->kwin_dbus_service);
    }
}

void DBusInterface::announceService()
{
    if (!kwinApp()->x11Connection()) {
        return;
    }
    const QByteArray service = m_serviceName.toUtf8();
    xcb_change_property(kwinApp()->x11Connection(), XCB_PROP_MODE_REPLACE, kwinApp()->x11RootWindow(), atoms->kwin_dbus_service,
                        atoms->utf8_string, 8, service.size(), service.constData());
}

// wrap void methods with no arguments to Workspace
#define WRAP(name) \
void DBusInterface::name() \
{\
    Workspace::self()->name();\
}

WRAP(reconfigure)

#undef WRAP

void DBusInterface::killWindow()
{
    Workspace::self()->slotKillWindow();
}

#define WRAP(name) \
void DBusInterface::name() \
{\
    Placement::self()->name();\
}

WRAP(cascadeDesktop)
WRAP(unclutterDesktop)

#undef WRAP

// wrap returning methods with no arguments to Workspace
#define WRAP( rettype, name ) \
rettype DBusInterface::name( ) \
{\
    return Workspace::self()->name(); \
}

WRAP(QString, supportInformation)

#undef WRAP

bool DBusInterface::startActivity(const QString &in0)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return false;
    }
    return Activities::self()->start(in0);
#else
    Q_UNUSED(in0)
    return false;
#endif
}

bool DBusInterface::stopActivity(const QString &in0)
{
#ifdef KWIN_BUILD_ACTIVITIES
    if (!Activities::self()) {
        return false;
    }
    return Activities::self()->stop(in0);
#else
    Q_UNUSED(in0)
    return false;
#endif
}

int DBusInterface::currentDesktop()
{
    return VirtualDesktopManager::self()->current();
}

bool DBusInterface::setCurrentDesktop(int desktop)
{
    return VirtualDesktopManager::self()->setCurrent(desktop);
}

void DBusInterface::nextDesktop()
{
    VirtualDesktopManager::self()->moveTo<DesktopNext>();
}

void DBusInterface::previousDesktop()
{
    VirtualDesktopManager::self()->moveTo<DesktopPrevious>();
}

void DBusInterface::showDebugConsole()
{
    DebugConsole *console = new DebugConsole;
    console->show();
}

QVariantMap DBusInterface::queryWindowInfo()
{
    m_replyQueryWindowInfo = message();
    setDelayedReply(true);
    kwinApp()->platform()->startInteractiveWindowSelection(
        [this] (Toplevel *t) {
            if (auto c = qobject_cast<AbstractClient*>(t)) {
                const QVariantMap ret{
                    {QStringLiteral("resourceClass"), c->resourceClass()},
                    {QStringLiteral("resourceName"), c->resourceName()},
                    {QStringLiteral("desktopFile"), c->desktopFileName()},
                    {QStringLiteral("role"), c->windowRole()},
                    {QStringLiteral("caption"), c->captionNormal()},
                    {QStringLiteral("clientMachine"), c->wmClientMachine(true)},
                    {QStringLiteral("type"), c->windowType()},
                    {QStringLiteral("x"), c->x()},
                    {QStringLiteral("y"), c->y()},
                    {QStringLiteral("width"), c->width()},
                    {QStringLiteral("height"), c->height()},
                    {QStringLiteral("x11DesktopNumber"), c->desktop()},
                    {QStringLiteral("minimized"), c->isMinimized()},
                    {QStringLiteral("shaded"), c->isShade()},
                    {QStringLiteral("fullscreen"), c->isFullScreen()},
                    {QStringLiteral("keepAbove"), c->keepAbove()},
                    {QStringLiteral("keepBelow"), c->keepBelow()},
                    {QStringLiteral("noBorder"), c->noBorder()},
                    {QStringLiteral("skipTaskbar"), c->skipTaskbar()},
                    {QStringLiteral("skipPager"), c->skipPager()},
                    {QStringLiteral("skipSwitcher"), c->skipSwitcher()},
                    {QStringLiteral("maximizeHorizontal"), c->maximizeMode() & MaximizeHorizontal},
                    {QStringLiteral("maximizeVertical"), c->maximizeMode() & MaximizeVertical}
                };
                QDBusConnection::sessionBus().send(m_replyQueryWindowInfo.createReply(ret));
            } else {
                QDBusConnection::sessionBus().send(m_replyQueryWindowInfo.createErrorReply(QString(), QString()));
            }
        }
    );
    return QVariantMap{};
}

CompositorDBusInterface::CompositorDBusInterface(Compositor *parent)
    : QObject(parent)
    , m_compositor(parent)
{
    connect(m_compositor, &Compositor::compositingToggled, this, &CompositorDBusInterface::compositingToggled);
    new CompositingAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject(QStringLiteral("/Compositor"), this);
    dbus.connect(QString(), QStringLiteral("/Compositor"), QStringLiteral("org.kde.kwin.Compositing"),
                 QStringLiteral("reinit"), m_compositor, SLOT(slotReinitialize()));
}

QString CompositorDBusInterface::compositingNotPossibleReason() const
{
    return kwinApp()->platform()->compositingNotPossibleReason();
}

QString CompositorDBusInterface::compositingType() const
{
    if (!m_compositor->hasScene()) {
        return QStringLiteral("none");
    }
    switch (m_compositor->scene()->compositingType()) {
    case XRenderCompositing:
        return QStringLiteral("xrender");
    case OpenGL2Compositing:
        if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
            return QStringLiteral("gles");
        } else {
            return QStringLiteral("gl2");
        }
    case QPainterCompositing:
        return QStringLiteral("qpainter");
    case NoCompositing:
    default:
        return QStringLiteral("none");
    }
}

bool CompositorDBusInterface::isActive() const
{
    return m_compositor->isActive();
}

bool CompositorDBusInterface::isCompositingPossible() const
{
    return kwinApp()->platform()->compositingPossible();
}

bool CompositorDBusInterface::isOpenGLBroken() const
{
    return kwinApp()->platform()->openGLCompositingIsBroken();
}

bool CompositorDBusInterface::platformRequiresCompositing() const
{
    return kwinApp()->platform()->requiresCompositing();
}

void CompositorDBusInterface::resume()
{
    m_compositor->resume(Compositor::ScriptSuspend);
}

void CompositorDBusInterface::suspend()
{
    m_compositor->suspend(Compositor::ScriptSuspend);
}

QStringList CompositorDBusInterface::supportedOpenGLPlatformInterfaces() const
{
    QStringList interfaces;
    bool supportsGlx = false;
#if HAVE_EPOXY_GLX
    supportsGlx = (kwinApp()->operationMode() == Application::OperationModeX11);
#endif
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
        supportsGlx = false;
    }
    if (supportsGlx) {
        interfaces << QStringLiteral("glx");
    }
    interfaces << QStringLiteral("egl");
    return interfaces;
}

} // namespace
