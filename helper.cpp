// Copyright (C) 2026 guantamanera <guantamanera@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://gnu.org>.
#include "devices.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>
#include <algorithm>
#include <unistd.h>

namespace {
int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

int run(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(5000))
        return fail(QStringLiteral("Cannot start %1: %2").arg(program, process.errorString()));
    if (!process.waitForFinished(60000)) {
        process.kill();
        process.waitForFinished();
        return fail(QStringLiteral("%1 timed out; inspect the server/device state before retrying.").arg(program));
    }
    QTextStream(stdout) << process.readAllStandardOutput();
    QTextStream(stderr) << process.readAllStandardError();
    if (process.exitStatus() != QProcess::NormalExit)
        return fail(QStringLiteral("%1 terminated unexpectedly.").arg(program));
    if (process.exitCode() != 0)
        return fail(QStringLiteral("%1 failed (exit %2).").arg(program).arg(process.exitCode()));
    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const QStringList args = application.arguments().mid(1);
    if (args.isEmpty())
        return fail("Expected start, stop, share BUSID, or unshare BUSID.");
    const QString command = args.first();
    const bool serverAction = command == "start" || command == "stop";
    const bool deviceAction = command == "share" || command == "unshare";
    if ((!serverAction && !deviceAction)
        || (serverAction && args.size() != 1)
        || (deviceAction && (args.size() != 2 || !validBusId(args.at(1)))))
        return fail("Invalid operation or USB bus ID.");
    if (geteuid() != 0)
        return fail("Administrative authorization is required. Run this helper through pkexec.");

    if (command == "stop")
        return run("/usr/bin/systemctl", {"stop", "usbipd.service"});
    if (command == "start") {
        if (!QFileInfo::exists("/usr/bin/usbipd"))
            return fail("USB/IP is not installed. Install the usbip package.");
        if (run("/usr/bin/modprobe", {"usbip_host"}) != 0)
            return 1;
        return run("/usr/bin/systemctl", {"start", "usbipd.service"});
    }

    const QString busId = args.at(1);
    const auto devices = readDevices();
    const auto found = std::find_if(devices.cbegin(), devices.cend(),
        [&busId](const UsbDevice &device) { return device.busId == busId; });
    if (found == devices.cend())
        return fail("The USB device is no longer present.");
    if (found->hub)
        return fail("USB hubs cannot be shared.");
    if (command == "share" && found->shared)
        return fail("The device is already shared.");
    if (command == "unshare" && !found->shared)
        return fail("The device is not shared.");
    if (command == "share" && run("/usr/bin/modprobe", {"usbip_host"}) != 0)
        return 1;
    return run("/usr/bin/usbip",
        {command == "share" ? "bind" : "unbind", "--busid=" + busId});
}
