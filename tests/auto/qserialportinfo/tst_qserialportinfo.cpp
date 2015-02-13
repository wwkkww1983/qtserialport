/****************************************************************************
**
** Copyright (C) 2014 Denis Shienkov <denis.shienkov@gmail.com>
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

#include <QtTest/QtTest>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

class tst_QSerialPortInfo : public QObject
{
    Q_OBJECT
public:
    explicit tst_QSerialPortInfo();

private slots:
    void initTestCase();

    void constructors();
    void assignment();

private:
    QString m_senderPortName;
    QString m_receiverPortName;
    QStringList m_availablePortNames;
};

tst_QSerialPortInfo::tst_QSerialPortInfo()
{
}

void tst_QSerialPortInfo::initTestCase()
{
    m_senderPortName = QString::fromLocal8Bit(qgetenv("QTEST_SERIALPORT_SENDER"));
    m_receiverPortName = QString::fromLocal8Bit(qgetenv("QTEST_SERIALPORT_RECEIVER"));
    if (m_senderPortName.isEmpty() || m_receiverPortName.isEmpty()) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        QSKIP("Test doesn't work because the names of serial ports aren't found in env.");
#else
        QSKIP("Test doesn't work because the names of serial ports aren't set found env.", SkipAll);
#endif
    } else {
        m_availablePortNames << m_senderPortName << m_receiverPortName;
    }
}

void tst_QSerialPortInfo::constructors()
{
    QSerialPortInfo empty;
    QVERIFY(empty.isNull());
    QSerialPortInfo empty2(QLatin1String("ABCD"));
    QVERIFY(empty2.isNull());
    QSerialPortInfo empty3(empty);
    QVERIFY(empty3.isNull());

    QSerialPortInfo exist(m_senderPortName);
    QVERIFY(!exist.isNull());
    QSerialPortInfo exist2(exist);
    QVERIFY(!exist2.isNull());
}

void tst_QSerialPortInfo::assignment()
{
    QSerialPortInfo empty;
    QVERIFY(empty.isNull());
    QSerialPortInfo empty2;
    empty2 = empty;
    QVERIFY(empty2.isNull());

    QSerialPortInfo exist(m_senderPortName);
    QVERIFY(!exist.isNull());
    QSerialPortInfo exist2;
    exist2 = exist;
    QVERIFY(!exist2.isNull());
}

QTEST_MAIN(tst_QSerialPortInfo)
#include "tst_qserialportinfo.moc"
