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
#pragma once

#include <QString>
#include <QVector>

struct UsbDevice
{
    QString busId;
    QString name;
    QString vendorProduct;
    QString driver;
    bool hub = false;
    bool inputDevice = false;
    bool shared = false;
    QString status;
};

bool validBusId(const QString &busId);
QVector<UsbDevice> readDevices(const QString &root = QStringLiteral("/sys/bus/usb/devices"));
