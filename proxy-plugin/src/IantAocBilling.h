// All code (c)2010-2015 IANT GmbH all rights reserved
// Contributed to eZuce under a Contributor Agreement
//
// This software is free software; you can redistribute it and/or modify it under
// the terms of the Affero General Public License (AGPL) as published by the
// Free Software Foundation; either version 3 of the License, or (at your option)
// any later version.
//
// This software is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.


#ifndef IantAocBilling_H_INCLUDED
#define IantAocBilling_H_INCLUDED

#include <net/SipBidirectionalProcessorPlugin.h>
#include <sipdb/MongoDB.h>
#include <string>
#include <net/SipMessage.h>

extern "C" SipBidirectionalProcessorPlugin* getTransactionPlugin(const UtlString& pluginName);

class IantAocBilling : public SipBidirectionalProcessorPlugin, MongoDB::BaseDB
{
public:
	/// destructor
	virtual ~IantAocBilling();

	virtual void initialize();

	virtual void readConfig( OsConfigDb& configDb  );

	virtual void handleIncoming(SipMessage& message, const char* address, int port);

	virtual void handleOutgoing(SipMessage& message, const char* address, int port);

private:
	std::string aocParser(const std::string& xml);
	std::string getAmount(const std::string& xml);
	void insertDataToMongoDb(const UtlString& callId, const UtlString& amount);
	bool checkContentType(SipMessage& message);
	void parseInformationsFromSipMessage(SipMessage& message);
	boost::regex regExAmount;

protected:
	IantAocBilling(const UtlString& instanceName, int priority, const MongoDB::ConnectionInfo& info);
	friend SipBidirectionalProcessorPlugin* getTransactionPlugin(const UtlString& pluginName);
};

#endif // IantAocBilling_H_INCLUDED
