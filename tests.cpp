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
#include <QTemporaryDir>
#include <QtTest>

class DeviceTests : public QObject
{
    Q_OBJECT
private slots:
    void busIds()
    {
        QVERIFY(validBusId("1-2"));
        QVERIFY(validBusId("12-3.4.5"));
        for (const auto &id : {"", "../1-2", "usb1", "1-2:1.0", "-1-2", "1-2\n", "1-2;reboot"})
            QVERIFY2(!validBusId(QString::fromLatin1(id)), id);
        QVERIFY(!validBusId(QString(32, '1') + "-2"));
    }

    void discovery()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QDir root(temporary.path());
        auto write = [&root](const QString &name, const QByteArray &value) {
            QFile file(root.filePath(name));
            if (!file.open(QIODevice::WriteOnly))
                return false;
            return file.write(value) == value.size();
        };
        QVERIFY(root.mkpath("1-2"));
        QVERIFY(root.mkpath("1-2:1.0"));
        QVERIFY(root.mkpath("usb1"));
        QVERIFY(root.mkpath("2-1"));
        QVERIFY(root.mkpath("3-1"));
        QVERIFY(write("1-2/idVendor", "1234\n"));
        QVERIFY(write("1-2/idProduct", "abcd\n"));
        QVERIFY(write("1-2/product", "Keyboard\n"));
        QVERIFY(write("1-2/manufacturer", "Example\n"));
        QVERIFY(write("1-2/bDeviceClass", "00\n"));
        QVERIFY(write("1-2:1.0/bInterfaceClass", "03\n"));
        QVERIFY(write("2-1/idVendor", "1111"));
        QVERIFY(write("2-1/idProduct", "2222"));
        QVERIFY(write("2-1/bDeviceClass", "09"));
        QVERIFY(write("3-1/idVendor", "3333"));
        QVERIFY(write("3-1/idProduct", "4444"));
        QVERIFY(write("3-1/usbip_status", "2"));
        QVERIFY(root.mkdir("usbip-host"));
        QVERIFY(QFile::link(root.filePath("usbip-host"), root.filePath("3-1/driver")));
        const auto devices = readDevices(temporary.path());
        QCOMPARE(devices.size(), 3);
        QCOMPARE(devices.at(0).name, QString("Example Keyboard"));
        QCOMPARE(devices.at(0).vendorProduct, QString("1234:abcd"));
        QVERIFY(devices.at(0).inputDevice);
        QVERIFY(!devices.at(0).shared);
        QVERIFY(devices.at(1).hub);
        QVERIFY(devices.at(2).shared);
        QCOMPARE(devices.at(2).status, QString("Shared - client attached"));
    }
};

QTEST_GUILESS_MAIN(DeviceTests)
#include "tests.moc"
