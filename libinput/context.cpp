/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2014 Martin Gräßlin <mgraesslin@kde.org>

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
#include "context.h"
#include "events.h"

#include <libudev.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

namespace KWin
{
namespace LibInput
{

Udev::Udev()
    : m_udev(udev_new())
{
}

Udev::~Udev()
{
    if (m_udev) {
        udev_unref(m_udev);
    }
}

Context::Context(const Udev &udev)
    : m_libinput(libinput_udev_create_context(&Context::s_interface, this, udev))
{
    libinput_log_set_priority(m_libinput, LIBINPUT_LOG_PRIORITY_DEBUG);
}

Context::~Context()
{
    if (m_libinput) {
        libinput_unref(m_libinput);
    }
}

bool Context::assignSeat(const char *seat)
{
    if (!isValid()) {
        return false;
    }
    return libinput_udev_assign_seat(m_libinput, seat) == 0;
}

int Context::fileDescriptor()
{
    if (!isValid()) {
        return 0;
    }
    return libinput_get_fd(m_libinput);
}

void Context::dispatch()
{
    libinput_dispatch(m_libinput);
}

const struct libinput_interface Context::s_interface = {
    Context::openRestrictedCallback,
    Context::closeRestrictedCallBack
};

int Context::openRestrictedCallback(const char *path, int flags, void *user_data)
{
    return ((Context*)user_data)->openRestricted(path, flags);
}

void Context::closeRestrictedCallBack(int fd, void *user_data)
{
    ((Context*)user_data)->closeRestricted(fd);
}

int Context::openRestricted(const char *path, int flags)
{
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

void Context::closeRestricted(int fd)
{
    close(fd);
}

Event *Context::event()
{
    return Event::create(libinput_get_event(m_libinput));
}

}
}