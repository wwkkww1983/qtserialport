/****************************************************************************
**
** Copyright (C) 2011-2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2011 Sergey Belyashov <Sergey.Belyashov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qserialportinfo.h"
#include "qserialportinfo_p.h"
#include "qserialport_p.h"

#include <QtCore/qlockfile.h>
#include <QtCore/qfile.h>
#include <QtCore/qdir.h>
#include <QtCore/qscopedpointer.h>


#include <errno.h>
#include <sys/types.h> // kill
#include <signal.h>    // kill

#include "qtudev_p.h"

QT_BEGIN_NAMESPACE

static QStringList filteredDeviceFilePaths()
{
    static const QStringList deviceFileNameFilterList = QStringList()

#ifdef Q_OS_LINUX
    << QStringLiteral("ttyS*")    // Standard UART 8250 and etc.
    << QStringLiteral("ttyO*")    // OMAP UART 8250 and etc.
    << QStringLiteral("ttyUSB*")  // Usb/serial converters PL2303 and etc.
    << QStringLiteral("ttyACM*")  // CDC_ACM converters (i.e. Mobile Phones).
    << QStringLiteral("ttyGS*")   // Gadget serial device (i.e. Mobile Phones with gadget serial driver).
    << QStringLiteral("ttyMI*")   // MOXA pci/serial converters.
    << QStringLiteral("ttymxc*")  // Motorola IMX serial ports (i.e. Freescale i.MX).
    << QStringLiteral("ttyAMA*")  // AMBA serial device for embedded platform on ARM (i.e. Raspberry Pi).
    << QStringLiteral("rfcomm*")  // Bluetooth serial device.
    << QStringLiteral("ircomm*"); // IrDA serial device.
#elif defined (Q_OS_FREEBSD)
    << QStringLiteral("cu*");
#elif defined (Q_OS_QNX)
    << QStringLiteral("ser*");
#else
    ;
#endif

    QStringList result;

    QDir deviceDir(QStringLiteral("/dev"));
    if (deviceDir.exists()) {
        deviceDir.setNameFilters(deviceFileNameFilterList);
        deviceDir.setFilter(QDir::Files | QDir::System | QDir::NoSymLinks);
        QStringList deviceFilePaths;
        foreach (const QFileInfo &deviceFileInfo, deviceDir.entryInfoList()) {
            const QString deviceAbsoluteFilePath = deviceFileInfo.absoluteFilePath();
            if (!deviceFilePaths.contains(deviceAbsoluteFilePath)) {
                deviceFilePaths.append(deviceAbsoluteFilePath);
                result.append(deviceAbsoluteFilePath);
            }
        }
    }

    return result;
}

QList<QSerialPortInfo> availablePortsByFiltersOfDevices()
{
    QList<QSerialPortInfo> serialPortInfoList;

    foreach (const QString &deviceFilePath, filteredDeviceFilePaths()) {
        QSerialPortInfoPrivate priv;
        priv.device = deviceFilePath;
        priv.portName = QSerialPortInfoPrivate::portNameFromSystemLocation(deviceFilePath);
        serialPortInfoList.append(priv);
    }

    return serialPortInfoList;
}

QList<QSerialPortInfo> availablePortsBySysfs()
{
    QDir ttySysClassDir(QStringLiteral("/sys/class/tty"));

    if (!(ttySysClassDir.exists() && ttySysClassDir.isReadable()))
        return QList<QSerialPortInfo>();

    QList<QSerialPortInfo> serialPortInfoList;
    ttySysClassDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    foreach (const QFileInfo &fileInfo, ttySysClassDir.entryInfoList()) {
        if (!fileInfo.isSymLink())
            continue;

        const QString targetPath = fileInfo.symLinkTarget();
        const int lastIndexOfSlash = targetPath.lastIndexOf(QLatin1Char('/'));
        if (lastIndexOfSlash == -1)
            continue;

        QSerialPortInfoPrivate priv;
        if (targetPath.contains(QStringLiteral("pnp"))) {
            // TODO: Obtain more information
#ifndef Q_OS_ANDROID
        } else if (targetPath.contains(QStringLiteral("platform"))) {
#else
        } else if (targetPath.contains(QStringLiteral("platform")) && !targetPath.contains(QStringLiteral("ttyUSB")) ) {
#endif
            continue;
        } else if (targetPath.contains(QStringLiteral("usb"))) {
            QDir targetDir(targetPath);
            targetDir.setFilter(QDir::Files | QDir::Readable);
            targetDir.setNameFilters(QStringList(QStringLiteral("uevent")));

            do {
                const QFileInfoList entryInfoList = targetDir.entryInfoList();
                if (entryInfoList.isEmpty())
                    continue;

                QFile uevent(entryInfoList.first().absoluteFilePath());
                if (!uevent.open(QIODevice::ReadOnly | QIODevice::Text))
                    continue;

                const QString ueventContent = QString::fromLatin1(uevent.readAll());

                if (ueventContent.contains(QStringLiteral("DEVTYPE=usb_device"))
                        && ueventContent.contains(QStringLiteral("DRIVER=usb"))) {

                    QFile description(QFileInfo(targetDir, QStringLiteral("product")).absoluteFilePath());
                    if (description.open(QIODevice::ReadOnly | QIODevice::Text))
                        priv.description = QString::fromLatin1(description.readAll()).simplified();

                    QFile manufacturer(QFileInfo(targetDir, QStringLiteral("manufacturer")).absoluteFilePath());
                    if (manufacturer.open(QIODevice::ReadOnly | QIODevice::Text))
                        priv.manufacturer = QString::fromLatin1(manufacturer.readAll()).simplified();

                    QFile serialNumber(QFileInfo(targetDir, QStringLiteral("serial")).absoluteFilePath());
                    if (serialNumber.open(QIODevice::ReadOnly | QIODevice::Text))
                        priv.serialNumber = QString::fromLatin1(serialNumber.readAll()).simplified();

                    QFile vendorIdentifier(QFileInfo(targetDir, QStringLiteral("idVendor")).absoluteFilePath());
                    if (vendorIdentifier.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        priv.vendorIdentifier = QString::fromLatin1(vendorIdentifier.readAll())
                                .toInt(&priv.hasVendorIdentifier, 16);
                    }

                    QFile productIdentifier(QFileInfo(targetDir, QStringLiteral("idProduct")).absoluteFilePath());
                    if (productIdentifier.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        priv.productIdentifier = QString::fromLatin1(productIdentifier.readAll())
                                .toInt(&priv.hasProductIdentifier, 16);
                    }

                    break;
                }
            } while (targetDir.cdUp());

        } else if (targetPath.contains(QStringLiteral("pci"))) {
            QDir targetDir(targetPath + QStringLiteral("/device"));
            QFile vendorIdentifier(QFileInfo(targetDir, QStringLiteral("vendor")).absoluteFilePath());
            if (vendorIdentifier.open(QIODevice::ReadOnly | QIODevice::Text)) {
                priv.vendorIdentifier = QString::fromLatin1(vendorIdentifier.readAll())
                        .toInt(&priv.hasVendorIdentifier, 16);
            }
            QFile productIdentifier(QFileInfo(targetDir, QStringLiteral("device")).absoluteFilePath());
            if (productIdentifier.open(QIODevice::ReadOnly | QIODevice::Text)) {
                priv.productIdentifier = QString::fromLatin1(productIdentifier.readAll())
                        .toInt(&priv.hasProductIdentifier, 16);
            }
            // TODO: Obtain more information about the device
        } else if (targetPath.contains(QStringLiteral(".serial/tty/tty"))) {
            // This condition matches onboard serial port on embedded devices.
            // Keep those devices in the list
        } else {
            continue;
        }

        priv.portName = targetPath.mid(lastIndexOfSlash + 1);
        priv.device = QSerialPortInfoPrivate::portNameToSystemLocation(priv.portName);
        serialPortInfoList.append(priv);
    }

    return serialPortInfoList;
}

struct ScopedPointerUdevDeleter
{
    static inline void cleanup(struct ::udev *pointer)
    {
        ::udev_unref(pointer);
    }
};

struct ScopedPointerUdevEnumeratorDeleter
{
    static inline void cleanup(struct ::udev_enumerate *pointer)
    {
        ::udev_enumerate_unref(pointer);
    }
};

struct ScopedPointerUdevDeviceDeleter
{
    static inline void cleanup(struct ::udev_device *pointer)
    {
        ::udev_device_unref(pointer);
    }
};

#ifndef LINK_LIBUDEV
    Q_GLOBAL_STATIC(QLibrary, udevLibrary)
#endif

static
QString getUdevPropertyValue(struct ::udev_device *dev, const char *name)
{
    return QString::fromLatin1(::udev_device_get_property_value(dev, name));
}

static
bool checkUdevForSerial8250Driver(struct ::udev_device *dev)
{
    const QString driverName = QString::fromLatin1(::udev_device_get_driver(dev));
    return (driverName == QStringLiteral("serial8250"));
}

static
QString getUdevModelName(struct ::udev_device *dev)
{
    return getUdevPropertyValue(dev, "ID_MODEL")
            .replace(QLatin1Char('_'), QLatin1Char(' '));
}

static
QString getUdevVendorName(struct ::udev_device *dev)
{
    return getUdevPropertyValue(dev, "ID_VENDOR")
            .replace(QLatin1Char('_'), QLatin1Char(' '));
}

static
quint16 getUdevModelIdentifier(struct ::udev_device *dev, bool &hasIdentifier)
{
    return getUdevPropertyValue(dev, "ID_MODEL_ID").toInt(&hasIdentifier, 16);
}

static
quint16 getUdevVendorIdentifier(struct ::udev_device *dev, bool &hasIdentifier)
{
    return getUdevPropertyValue(dev, "ID_VENDOR_ID").toInt(&hasIdentifier, 16);
}

static
QString getUdevSerialNumber(struct ::udev_device *dev)
{
    return getUdevPropertyValue(dev,"ID_SERIAL_SHORT");
}

QList<QSerialPortInfo> availablePortsByUdev()
{
#ifndef LINK_LIBUDEV
    static bool symbolsResolved = resolveSymbols(udevLibrary());
    if (!symbolsResolved)
        return QList<QSerialPortInfo>();
#endif
    static const QString rfcommDeviceName(QStringLiteral("rfcomm"));

    QScopedPointer<struct ::udev, ScopedPointerUdevDeleter> udev(::udev_new());

    if (!udev)
        return QList<QSerialPortInfo>();

    QScopedPointer<udev_enumerate, ScopedPointerUdevEnumeratorDeleter>
            enumerate(::udev_enumerate_new(udev.data()));

    if (!enumerate)
        return QList<QSerialPortInfo>();

    ::udev_enumerate_add_match_subsystem(enumerate.data(), "tty");
    ::udev_enumerate_scan_devices(enumerate.data());

    udev_list_entry *devices = ::udev_enumerate_get_list_entry(enumerate.data());

    QList<QSerialPortInfo> serialPortInfoList;
    udev_list_entry *dev_list_entry;
    udev_list_entry_foreach(dev_list_entry, devices) {

        QScopedPointer<udev_device, ScopedPointerUdevDeviceDeleter>
                dev(::udev_device_new_from_syspath(
                        udev.data(), ::udev_list_entry_get_name(dev_list_entry)));

        if (!dev)
            return serialPortInfoList;

        QSerialPortInfoPrivate priv;

        priv.device = QString::fromLatin1(::udev_device_get_devnode(dev.data()));
        priv.portName = QString::fromLatin1(::udev_device_get_sysname(dev.data()));

        udev_device *parentdev = ::udev_device_get_parent(dev.data());

        if (parentdev) {
            if (checkUdevForSerial8250Driver(parentdev))
                continue;
            priv.description = getUdevModelName(dev.data());
            priv.manufacturer = getUdevVendorName(dev.data());
            priv.serialNumber = getUdevSerialNumber(dev.data());
            priv.vendorIdentifier = getUdevVendorIdentifier(dev.data(), priv.hasVendorIdentifier);
            priv.productIdentifier = getUdevModelIdentifier(dev.data(), priv.hasProductIdentifier);
        } else {
            if (priv.portName.startsWith(rfcommDeviceName)) {
                bool ok;
                int portNumber = priv.portName.mid(rfcommDeviceName.length()).toInt(&ok);
                if (!ok || (portNumber < 0) || (portNumber > 255))
                    continue;
            } else {
                continue;
            }
        }

        serialPortInfoList.append(priv);
    }

    return serialPortInfoList;
}

QList<QSerialPortInfo> QSerialPortInfo::availablePorts()
{
    QList<QSerialPortInfo> serialPortInfoList = availablePortsByUdev();

#ifdef Q_OS_LINUX
    if (serialPortInfoList.isEmpty())
        serialPortInfoList = availablePortsBySysfs();
    else
        return serialPortInfoList;
#endif

    if (serialPortInfoList.isEmpty())
        serialPortInfoList = availablePortsByFiltersOfDevices();

    return serialPortInfoList;
}

QList<qint32> QSerialPortInfo::standardBaudRates()
{
    return QSerialPortPrivate::standardBaudRates();
}

bool QSerialPortInfo::isBusy() const
{
    QString lockFilePath = serialPortLockFilePath(portName());
    if (lockFilePath.isEmpty())
        return false;

    QFile reader(lockFilePath);
    if (!reader.open(QIODevice::ReadOnly))
        return false;

    QByteArray pidLine = reader.readLine();
    pidLine.chop(1);
    if (pidLine.isEmpty())
        return false;

    qint64 pid = pidLine.toLongLong();

    if (pid && (::kill(pid, 0) == -1) && (errno == ESRCH))
        return false; // PID doesn't exist anymore

    return true;
}

bool QSerialPortInfo::isValid() const
{
    QFile f(systemLocation());
    return f.exists();
}

QString QSerialPortInfoPrivate::portNameToSystemLocation(const QString &source)
{
    return (source.startsWith(QLatin1Char('/'))
            || source.startsWith(QStringLiteral("./"))
            || source.startsWith(QStringLiteral("../")))
            ? source : (QStringLiteral("/dev/") + source);
}

QString QSerialPortInfoPrivate::portNameFromSystemLocation(const QString &source)
{
    return source.startsWith(QStringLiteral("/dev/"))
            ? source.mid(5) : source;
}

QT_END_NAMESPACE
