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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace {
QString readAttribute(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll()).trimmed();
}
}

bool validBusId(const QString &busId)
{
    static const QRegularExpression pattern(QStringLiteral("\\A[0-9]+-[0-9]+(?:\\.[0-9]+)*\\z"));
    return busId.size() <= 31 && pattern.match(busId).hasMatch();
}

QVector<UsbDevice> readDevices(const QString &root)
{
    QVector<UsbDevice> devices;
    const QDir directory(root);
    for (const QString &busId : directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (!validBusId(busId))
            continue;
        const QString path = directory.filePath(busId);
        const QString vendor = readAttribute(path + "/idVendor");
        const QString product = readAttribute(path + "/idProduct");
        if (vendor.isEmpty() || product.isEmpty())
            continue;

        UsbDevice device;
        device.busId = busId;
        const QString manufacturer = readAttribute(path + "/manufacturer");
        const QString name = readAttribute(path + "/product");
        device.name = name.isEmpty() ? QStringLiteral("Unknown USB device") : name;
        if (!manufacturer.isEmpty() && !device.name.startsWith(manufacturer, Qt::CaseInsensitive))
            device.name.prepend(manufacturer + ' ');
        device.vendorProduct = vendor + ':' + product;
        device.driver = QFileInfo(QFileInfo(path + "/driver").symLinkTarget()).fileName();
        device.hub = readAttribute(path + "/bDeviceClass") == "09";
        device.shared = device.driver == "usbip-host";
        const QString usbipStatus = readAttribute(path + "/usbip_status");
        if (!device.shared)
            device.status = QStringLiteral("Local");
        else if (usbipStatus == "1")
            device.status = QStringLiteral("Shared - available");
        else if (usbipStatus == "2")
            device.status = QStringLiteral("Shared - client attached");
        else if (usbipStatus == "3")
            device.status = QStringLiteral("Shared - error");
        else
            device.status = QStringLiteral("Shared - status unknown");

        const QDir interfaces(root);
        for (const QString &entry : interfaces.entryList({busId + ":*"}, QDir::Dirs)) {
            if (readAttribute(interfaces.filePath(entry) + "/bInterfaceClass") == "03")
                device.inputDevice = true;
        }
        devices.append(device);
    }
    return devices;
}
