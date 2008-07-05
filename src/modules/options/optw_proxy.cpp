//
//   File : optw_proxy.cpp
//   Creation date : Mon Jun 24 2000 22:02:11 by Szymon Stefanek
//
//   This file is part of the KVirc irc client distribution
//   Copyright (C) 2000 Szymon Stefanek (pragma at kvirc dot net)
//
//   This program is FREE software. You can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your opinion) any later version.
//
//   This program is distributed in the HOPE that it will be USEFUL,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, write to the Free Software Foundation,
//   Inc. ,59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//

#include "optw_proxy.h"

#include "kvi_locale.h"
#include "kvi_iconmanager.h"
#include "kvi_proxydb.h"
#include "kvi_ipeditor.h"
#include "kvi_netutils.h"
#include "kvi_settings.h"
#include "kvi_options.h"
#include <kvi_tal_groupbox.h>
#include "kvi_tal_popupmenu.h"
#include "kvi_tal_tooltip.h"

#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCursor>
#include <QIcon>
#include <QToolButton>


KviProxyOptionsTreeWidgetItem::KviProxyOptionsTreeWidgetItem(KviTalTreeWidget *parent,const QPixmap &pm,KviProxy * prx)
: KviTalTreeWidgetItem(parent,prx->m_szHostname.ptr())
{
	debug("Creating item");
	setIcon(0,QIcon(pm));
	m_pProxyData = new KviProxy(*prx);
}

KviProxyOptionsTreeWidgetItem::~KviProxyOptionsTreeWidgetItem()
{
	debug("Deleting item");
	delete m_pProxyData;
}

KviProxyOptionsWidget::KviProxyOptionsWidget(QWidget * parent)
: KviOptionsWidget(parent,"proxy_options_widget")
{
	createLayout();

	addBoolSelector(0,0,1,0,__tr2qs_ctx("Use proxy","options"),KviOption_boolUseProxyHost);

	m_pTreeWidget = new KviTalTreeWidget(this);
	addWidgetToLayout(m_pTreeWidget,0,1,0,1);
	m_pTreeWidget->addColumn(__tr2qs_ctx("Proxy","options"));
	m_pTreeWidget->setRootIsDecorated(true);
	m_pTreeWidget->setAllColumnsShowFocus(true);
	m_pTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);

	connect(m_pTreeWidget,SIGNAL(currentItemChanged(KviTalTreeWidgetItem *,KviTalTreeWidgetItem *)),
		this,SLOT(currentItemChanged(KviTalTreeWidgetItem *,KviTalTreeWidgetItem *)));
	m_pTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTreeWidget,SIGNAL(customContextMenuRequested(const QPoint &)),
			this,SLOT(customContextMenuRequested(const QPoint &)));

	QString tiptxt = __tr2qs_ctx("<center>This is the list of available proxy servers.<br>" \
				"Right-click on the list to add or remove proxies.</center>","options");
	mergeTip(m_pTreeWidget,tiptxt);
	mergeTip(m_pTreeWidget->viewport(),tiptxt);

	KviTalVBox * vbox = new KviTalVBox(this);
	addWidgetToLayout(vbox,1,1,1,1);
	
	QToolButton * tb = new QToolButton(vbox);
	tb->setIcon(QIcon(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_PROXY))));
	tb->setAutoRaise(true);
	connect(tb,SIGNAL(clicked()),this,SLOT(newProxy()));
	mergeTip(tb,__tr2qs_ctx("New Proxy","options"));

	tb = new QToolButton(vbox);
	tb->setIcon(QIcon(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_CUT))));
	//tb->setEnabled(false);
	tb->setAutoRaise(true);
	connect(tb,SIGNAL(clicked()),this,SLOT(removeCurrent()));
	mergeTip(tb,__tr2qs_ctx("Remove Proxy","options"));
	
	QFrame * lll = new QFrame(vbox);
	vbox->setStretchFactor(lll,100);


	KviTalGroupBox * gbox = addGroupBox(0,2,1,2,Qt::Horizontal,__tr2qs_ctx("Configuration","options"),this);
	//QGridLayout * gl = new QGridLayout(gbox->layout());
	//gl->setMargin(2);
	//gl->setSpacing(4);

	m_pProxyLabel = new QLabel(__tr2qs_ctx("Proxy:","options"),gbox);
	m_pProxyEdit = new QLineEdit(gbox);
	m_pPortLabel = new QLabel(__tr2qs_ctx("Port:","options"),gbox);
	m_pPortEdit = new QLineEdit(gbox);
	m_pIpLabel = new QLabel(__tr2qs_ctx("IP address:","options"),gbox);
	m_pIpEditor = new KviIpEditor(gbox,KviIpEditor::IPv4);
	m_pUserLabel = new QLabel(__tr2qs_ctx("Username:","options"),gbox);
	m_pUserEdit = new QLineEdit(gbox);
	m_pPassLabel = new QLabel(__tr2qs_ctx("Password:","options"),gbox);
	m_pPassEdit = new QLineEdit(gbox);
	m_pProtocolLabel = new QLabel(__tr2qs_ctx("Protocol:","options"),gbox);
	m_pProtocolBox = new QComboBox(gbox);

	QStringList l;
	KviProxy::getSupportedProtocolNames(l);

	m_pProtocolBox->addItems(l);

	m_pIPv6Check = new QCheckBox(__tr2qs_ctx("Use IPv6 protocol","options"),gbox);
	connect(m_pIPv6Check,SIGNAL(toggled(bool)),this,SLOT(ipV6CheckToggled(bool)));
#ifndef COMPILE_IPV6_SUPPORT
	m_pIPv6Check->setEnabled(false);
#endif

	m_pLastEditedItem = 0;

	fillProxyList();

	layout()->setRowStretch(0,1);
	layout()->setColumnStretch(0,1);

	m_pContextPopup = new KviTalPopupMenu(this);
}

KviProxyOptionsWidget::~KviProxyOptionsWidget()
{
}

void KviProxyOptionsWidget::ipV6CheckToggled(bool bEnabled)
{
	m_pIpEditor->setAddressType(bEnabled ? KviIpEditor::IPv6 : KviIpEditor::IPv4);
}

void KviProxyOptionsWidget::fillProxyList()
{
	KviProxyOptionsTreeWidgetItem * prx;

	KviPointerList<KviProxy> * l = g_pProxyDataBase->proxyList();

	for(KviProxy * p = l->first();p;p = l->next())
	{
		prx = new KviProxyOptionsTreeWidgetItem(m_pTreeWidget,*(g_pIconManager->getSmallIcon(KVI_SMALLICON_PROXY)),p);
		if(p == g_pProxyDataBase->currentProxy())
		{
			prx->setSelected(true);
			m_pTreeWidget->scrollToItem(prx);
		}
	}
	if(!(g_pProxyDataBase->currentProxy()))currentItemChanged(0,0);
}

void KviProxyOptionsWidget::currentItemChanged(KviTalTreeWidgetItem *it,KviTalTreeWidgetItem *prev)
{
	if(m_pLastEditedItem)saveLastItem();
	m_pLastEditedItem = (KviProxyOptionsTreeWidgetItem *)it;

	m_pProxyLabel->setEnabled(m_pLastEditedItem);
	m_pProxyEdit->setEnabled(m_pLastEditedItem);
	m_pIpLabel->setEnabled(m_pLastEditedItem);
	m_pIpEditor->setEnabled(m_pLastEditedItem);
	m_pUserLabel->setEnabled(m_pLastEditedItem);
	m_pUserEdit->setEnabled(m_pLastEditedItem);
	m_pPassLabel->setEnabled(m_pLastEditedItem);
	m_pPassEdit->setEnabled(m_pLastEditedItem);
	m_pProtocolLabel->setEnabled(m_pLastEditedItem);
	m_pProtocolBox->setEnabled(m_pLastEditedItem);
	m_pPortLabel->setEnabled(m_pLastEditedItem);
	m_pPortEdit->setEnabled(m_pLastEditedItem);

#ifdef COMPILE_IPV6_SUPPORT
		m_pIPv6Check->setEnabled(m_pLastEditedItem);
#else
		m_pIPv6Check->setEnabled(false);
#endif
	if(m_pLastEditedItem)
	{
		m_pProxyEdit->setText(m_pLastEditedItem->m_pProxyData->m_szHostname.ptr());

		for(int i=0;i<m_pProtocolBox->count();i++)
		{
			KviStr txt = m_pProtocolBox->itemText(i);
			if(kvi_strEqualCI(m_pLastEditedItem->m_pProxyData->protocolName(),txt.ptr()))
			{
				m_pProtocolBox->setCurrentIndex(i);
				break;
			}
		}

#ifdef COMPILE_IPV6_SUPPORT
		m_pIPv6Check->setChecked(m_pLastEditedItem->m_pProxyData->isIPv6());
		m_pIpEditor->setAddressType(m_pLastEditedItem->m_pProxyData->isIPv6() ? KviIpEditor::IPv6 : KviIpEditor::IPv4);
#else
		m_pIPv6Check->setChecked(false);
		m_pIpEditor->setAddressType(KviIpEditor::IPv4);
#endif


		if(!m_pIpEditor->setAddress(m_pLastEditedItem->m_pProxyData->m_szIp.ptr()))
		{
#ifdef COMPILE_IPV6_SUPPORT
			m_pIpEditor->setAddress(m_pLastEditedItem->m_pProxyData->isIPv6() ? "0:0:0:0:0:0:0:0" : "0.0.0.0");
#else
			m_pIpEditor->setAddress("0.0.0.0");
#endif
		}

		m_pUserEdit->setText(m_pLastEditedItem->m_pProxyData->m_szUser.ptr());
		m_pPassEdit->setText(m_pLastEditedItem->m_pProxyData->m_szPass.ptr());
		KviStr tmp(KviStr::Format,"%u",m_pLastEditedItem->m_pProxyData->m_uPort);
		m_pPortEdit->setText(tmp.ptr());
	} else {
		m_pProxyEdit->setText("");
		m_pUserEdit->setText("");
		m_pPassEdit->setText("");
		m_pPortEdit->setText("");
		m_pIpEditor->setAddress("0.0.0.0");
		m_pIPv6Check->setEnabled(false);
	}
}

void KviProxyOptionsWidget::saveLastItem()
{
	if(m_pLastEditedItem)
	{
		KviStr tmp = m_pProxyEdit->text();
		if(tmp.isEmpty())tmp = "irc.unknown.net";
		m_pLastEditedItem->setText(0,tmp.ptr());
		m_pLastEditedItem->m_pProxyData->m_szHostname = tmp;
#ifdef COMPILE_IPV6_SUPPORT
		m_pLastEditedItem->m_pProxyData->m_bIsIPv6 = m_pIPv6Check->isChecked();
#else
		m_pLastEditedItem->m_pProxyData->m_bIsIPv6 = false;
#endif
		m_pLastEditedItem->m_pProxyData->m_szIp = "";
		KviStr tmpAddr = m_pIpEditor->address();

		if(!m_pIpEditor->hasEmptyFields())
		{
#ifdef COMPILE_IPV6_SUPPORT
			if(m_pIPv6Check->isChecked())
			{
				if((!kvi_strEqualCI(tmpAddr.ptr(),"0:0:0:0:0:0:0:0")) &&
					kvi_isValidStringIp_V6(tmpAddr.ptr()))
				{
					m_pLastEditedItem->m_pProxyData->m_szIp = tmpAddr;
				}
			} else {
#endif
				if((!kvi_strEqualCI(tmpAddr.ptr(),"0.0.0.0")) &&
					kvi_isValidStringIp(tmpAddr.ptr()))
				{
					m_pLastEditedItem->m_pProxyData->m_szIp = tmpAddr;
				}
#ifdef COMPILE_IPV6_SUPPORT
			}
#endif
		}

		m_pLastEditedItem->m_pProxyData->m_szPass = m_pPassEdit->text();
		m_pLastEditedItem->m_pProxyData->m_szUser = m_pUserEdit->text();
		tmp = m_pPortEdit->text();
		bool bOk;
		kvi_u32_t uPort = tmp.toUInt(&bOk);
		if(!bOk)uPort = 1080;
		m_pLastEditedItem->m_pProxyData->m_uPort = uPort;
//		m_pLastEditedItem->m_pProxyData->m_bSocksV5 = m_pSocks5Check->isChecked();
		tmp = m_pProtocolBox->currentText();
		m_pLastEditedItem->m_pProxyData->setNamedProtocol(tmp.ptr());
	}
}

void KviProxyOptionsWidget::commit()
{
	saveLastItem();
	g_pProxyDataBase->clear();
	KviProxyOptionsTreeWidgetItem * it;// = (KviProxyOptionsTreeWidgetItem *)m_pTreeWidget->topLevelItemCount();
	
	//while(it)
	for(int i=0;i<m_pTreeWidget->topLevelItemCount();i++)
	{
		it=(KviProxyOptionsTreeWidgetItem *)m_pTreeWidget->topLevelItem(i);	
		QString tmp = it->text(0);
		if(!tmp.isEmpty())
		{
			debug("Commit proxy name %s",tmp.toUtf8().data());
			KviProxy * prx = new KviProxy(*(it->m_pProxyData));
			g_pProxyDataBase->insertProxy(prx);

			if(it == m_pLastEditedItem)g_pProxyDataBase->setCurrentProxy(prx);
		}
		//it = (KviProxyOptionsTreeWidgetItem *)it->nextSibling();
	}

	if(g_pProxyDataBase->currentProxy() == 0)
	{
		g_pProxyDataBase->setCurrentProxy(g_pProxyDataBase->proxyList()->first());
	}

	KviOptionsWidget::commit();
}

void KviProxyOptionsWidget::customContextMenuRequested(const QPoint &pos)
{
	KviTalTreeWidgetItem *it=(KviTalTreeWidgetItem *)m_pTreeWidget->itemAt(pos);
	m_pContextPopup->clear();
	m_pContextPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_PROXY)),__tr2qs_ctx("&New Proxy","options"),this,SLOT(newProxy()));
	m_pContextPopup->setItemEnabled(m_pContextPopup->insertItem(*(g_pIconManager->getSmallIcon(KVI_SMALLICON_CUT)),__tr2qs_ctx("Re&move Proxy","options"),this,SLOT(removeCurrent())),it);
	m_pContextPopup->popup(QCursor::pos());
}

void KviProxyOptionsWidget::newProxy()
{
	KviProxy prx;
	KviProxyOptionsTreeWidgetItem * it = new KviProxyOptionsTreeWidgetItem(m_pTreeWidget,*(g_pIconManager->getSmallIcon(KVI_SMALLICON_PROXY)),&prx);
	it->setSelected(true);
	m_pTreeWidget->scrollToItem(it);
}

void KviProxyOptionsWidget::removeCurrent()
{
	if(m_pLastEditedItem)
	{
		delete m_pLastEditedItem;
		m_pLastEditedItem = 0;
		KviTalTreeWidgetItem * it = (KviTalTreeWidgetItem *)m_pTreeWidget->topLevelItem(0);
		if(it)
		{
			it->setSelected(true);
			//m_pTreeWidget->ensureItemVisible(it);
		} else {
			currentItemChanged(0,0);
		}
	}
}

#ifndef COMPILE_USE_STANDALONE_MOC_SOURCES
#include "m_optw_proxy.moc"
#endif //!COMPILE_USE_STANDALONE_MOC_SOURCES
