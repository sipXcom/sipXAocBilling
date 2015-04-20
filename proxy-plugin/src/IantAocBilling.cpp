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

#include <cassert>
#include <utl/UtlString.h>
#include <sys/time.h>
#include <sstream>
#include <string>
#include <stdlib.h>

#include "IantAocBilling.h"
#include "digitmaps/EmergencyRulesUrlMapping.h"
#include "sipXecsService/SipXecsService.h"
#include "os/OsLogger.h"
#include "os/OsConfigDb.h"
#include "os/OsFS.h"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>
#include <boost/scoped_ptr.hpp>
#include <mongo/client/connpool.h>
#include <mongo/client/dbclient.h>
#include <mongo/client/dbclient_rs.h>
#include "sipdb/MongoMod.h"

static const int SIPX_PLUGIN_PRIORITY = 991;
static const std::string COLLECTION_IANT_BILLING = "iant_billing";
static const std::string NAMESPACE_IANT_BILLING = "iant";

/**
 *  === General Code formatting and style ===
 *    - Keep the indentation to whether tabs, or 2/4 spaces
 *      The code is chaotically indented with tabs and spaces (2 and 4) which
 *      makes it very unpleasant to read. You should choose one desired
 *      indentation and keep to it; I recommend choosing the 2spaces one since
 *    - Use the same coding style everywhere
 *      The code style is also mixed, in special related to the spacing between
 *      operators (=,<> ..) and variable and, also, for reserved 'words' (i.e. if,
 *      for). I recommend that the conventional operators (i.e. =<>) to be
 *      surrounded by a space character, i.e. variable = value,
 *      for (i = 0; i < 10; i++) { // NOT: for(i=0;i<10;i++){ ...
 */

/// Factory used by PluginHooks to dynamically link the plugin instance
extern "C" SipBidirectionalProcessorPlugin* getTransactionPlugin(const UtlString& pluginName)
{
    MongoDB::ConnectionInfo global = MongoDB::ConnectionInfo::globalInfo();
    return new IantAocBilling(pluginName, SIPX_PLUGIN_PRIORITY, global);
}

IantAocBilling::IantAocBilling(const UtlString& instanceName, int priority, const MongoDB::ConnectionInfo& info) :
    SipBidirectionalProcessorPlugin(instanceName, priority),
    MongoDB::BaseDB(info,COLLECTION_IANT_BILLING)
{
}

IantAocBilling::~IantAocBilling()
{
}

void IantAocBilling::readConfig(OsConfigDb& configDb)
{
	OS_LOG_NOTICE(FAC_SIP, "IAB: IANT AOC Billing Plugin config Loaded");
}

void IantAocBilling::initialize()
{
	OS_LOG_NOTICE(FAC_SIP, "IAB: IANT AOC Billing Plugin initialized");
}

/*
*	Regex for Parsing amout out of xml body
*/
// REVIEW: use a const reference to the variable (e.g. const std::string& xml)
std::string IantAocBilling::getAmount(std::string xml)
{
  /**
   * REVIEW:
   *  1. use defines for the hardcoded strings
   *     (i.e. #define CURRENCY_AMOUNT_XML_TAG "currency-amount")
   *  2. 're' variable is constant for every function call, thus it cat be better
   *     declared in the constructor
   *  3. the regex will match "<currency-amount></currency-amount>" string which,
   *     i think, is not the desired result. Change it to specify at least 1char
   *     or, in case the currency-amount is designed to be a number, to expect
   *     at least 1 digit: (.+) or, respectively, ([0-9]+)
   *
   */
	boost::regex re(".*<currency-amount>(.*)<\\/currency-amount>.*");
	boost::cmatch matches;
	if(boost::regex_match(xml.c_str(), matches, re))
	{
	  // REVIEW: what's the meaning of this for? because it doesn't do anything,
	  //         it just gets the match for pos 1 and returns.
	  //         This should be changed with simply using the matches[1]
		for(uint16_t i=1; i<matches.size(); i++)
		{
		  // REVIEW: 'match' variable is just assigned, not used
			std::string match(matches[i].first, matches[i].second);
			// REVIEW: 'val' variable is dangerously assigned, it's pure luck that the
			//         std::string has std::pair as copy-constructor
			//         The 'val' variable should be simply replaced by the 'match' variable
			std::string val = matches[i];
			boost::trim(val);
			return val;
		}
	}
	return "";
}

/*
* Find Amount in XML from AOC-D and AOC-E
*/
// REVIEW: use a const reference to the variable (e.g. const std::string& xml)
std::string IantAocBilling::aocParser(std::string xml)
{
  // REVIEW: hardcoded strings should be replaced with defines
    try {
	if(boost::starts_with(xml,"<?xml"))
	{
	    if(boost::contains(xml,"<aoc xmlns"))
	    {
		if(boost::contains(xml,"<aoc-d") || boost::contains(xml,"<aoc-e"))
		{
		    if(boost::contains(xml,"<currency-amount>"))
		    {
			return getAmount(xml);
		    }
		}
	    }
	}
	return "";
    }
    catch (...)
    {
	return "";
    }
}

/*
* Insert Data into DB
*/
// REVIEW: use a const reference to the variables (e.g. const UtlString& callId and const UtlString& amount)
void IantAocBilling::insertDataToMongoDb(UtlString callId, UtlString amount)
{
    OS_LOG_DEBUG(FAC_SIP,"IAB: insertDataToMongoDb: "<< "CallID: "<<callId << " Amount: "<< amount << " Timeout is "<<  getReadQueryTimeout());
    // REVIEW: since the same mongo connection is used for both operations, reading and inserting, it's safer to set the writeQueryTimeout
    MongoDB::ScopedDbConnectionPtr conn(mongoMod::ScopedDbConnection::getScopedDbConnection(_info.getConnectionString().toString(), getReadQueryTimeout()));
    mongo::DBClientBase* dbc = conn->get();

    try
    {
	OS_LOG_DEBUG(FAC_SIP,"IAB: insertDataToMongoDb: Trying to Query ");
	mongo::BSONObj query = BSON("_id" << callId);
	mongo::BSONObj obj = dbc->findOne(NAMESPACE_IANT_BILLING + "." + COLLECTION_IANT_BILLING, query);
	int oldAmount = 0;
	int newAmount = 0;
	OS_LOG_DEBUG(FAC_SIP,"IAB: insertDataToMongoDb: Query Object is " << obj.toString());
	if (!obj.isEmpty() && obj.hasField("amount"))
	{
	    OS_LOG_DEBUG(FAC_SIP,"IAB: insertDataToMongoDb: Entry was already in MongoDB! ");
	    oldAmount = obj.getIntField("amount");
	}
	OS_LOG_DEBUG(FAC_SIP,"IAB: insertDataToMongoDb: Old Amount is "<<oldAmount);
	try
	{
	    OS_LOG_DEBUG(FAC_SIP,"IAB: insertDataToMongoDb: Parsing new Amount");
	    newAmount = atoi(amount);
	}
	catch(...)
	{
	    OS_LOG_DEBUG(FAC_SIP,"IAB: insertDataToMongoDb: Parsing new Amount failed");
	}
		
	if(oldAmount<newAmount)
	{
	    OS_LOG_DEBUG(FAC_SIP,"IAB: insertDataToMongoDb: Valid Amount ");
	    //REVIEW: do not create new strings from callId and amount, convert them to strings instead (i.e. callId.str())
	    mongo::BSONObj data = BSON("_id" << std::string(callId) << "amount" << std::string(amount));
	    dbc->update(NAMESPACE_IANT_BILLING + "." + COLLECTION_IANT_BILLING,query,data,true);
	}
	else
	{
	    OS_LOG_DEBUG(FAC_SIP,"IAB: insertDataToMongoDb: Invalid Amount ");
	}
	conn->done();
	OS_LOG_DEBUG(FAC_SIP,"IAB: insertDataToMongoDb: Done() ");
    }
    catch (...)
    {
	OS_LOG_DEBUG(FAC_SIP,"IAB: insertDataToMongoDb: Connection failed! ");
    }
}


void IantAocBilling::handleOutgoing(SipMessage& message, const char* address, int port)
{
    return;
}

void IantAocBilling::handleIncoming(SipMessage& message, const char* address, int port)
{
/*  if (!gPluginEnabled)
    return;*/
    OS_LOG_DEBUG(FAC_SIP,"IAB: Handle Incoming Message");
    if (!message.isResponse())
    {
	int seq = 0;
	UtlString method;
	// REVIEW: use getRequestMethod(&method) instead of getCSeqField; the last one is meant to be used when the message is response
	if (message.getCSeqField(&seq, &method)) // get method out of cseq field
	{
	    if (0 == method.compareTo(SIP_INFO_METHOD, UtlString::ignoreCase) || 0 == method.compareTo(SIP_BYE_METHOD, UtlString::ignoreCase))
	    {
		OS_LOG_DEBUG(FAC_SIP,"IAB: Method is Info or Bye");
		if(checkContentLengthAndType(message))
		{
		    OS_LOG_DEBUG(FAC_SIP,"IAB: Content Type is correct");
		    parseInformationsFromSipMessage(message);
		}
	    }
	}
    } else /*if (message.isResponse())*/
    {
	// Search for AOC in Responses 
	int responseCode = message.getResponseStatusCode();
	//Only for 1xx and 2xx responses check the record route and contact field of response
	if ( responseCode >= SIP_TRYING_CODE && responseCode < SIP_MULTI_CHOICE_CODE)
	{
	    OS_LOG_DEBUG(FAC_SIP,"IAB: Is valid Response Code: " << responseCode);
	    if(checkContentLengthAndType(message))
	    {
		OS_LOG_DEBUG(FAC_SIP,"IAB: Content Type is correct");
		parseInformationsFromSipMessage(message);
	    }
	}
    }
}

// REVIEW: function name is misleading - the Content-Length is not checked;
//         rename it to something like checkContentType
bool IantAocBilling::checkContentLengthAndType(SipMessage& message)
{
  // REVIEW: define for the hardcoded strings
    const char* contentType = message.getHeaderValue(0,"Content-Type");	
    if(contentType)
    {
	// Ok check if Content-Type: application/vnd.etsi.aoc+xml
	UtlString temp = UtlString(contentType);
	    return 0==temp.compareTo("application/vnd.etsi.aoc+xml", UtlString::ignoreCase);
    }
    return false;
}

void IantAocBilling::parseInformationsFromSipMessage(SipMessage& message)
{
    OS_LOG_DEBUG(FAC_SIP,"IAB: Parsing Informations from Message");

    // Correct Message, get Content and parse Informations and safe to DB
    // REVIEW: extract the callId only if the message body is not null -> move
    //         the code inside 'if(!body.isNull())' scope when it's actually needed
    UtlString callId;
    message.getCallIdField(&callId);
    OS_LOG_DEBUG(FAC_SIP,"IAB: CallID found "<< callId);
	
	// Get Body
    UtlString body = message.getBody()->getBytes();
    OS_LOG_DEBUG(FAC_SIP,"IAB: message.getString() found "<< message.getString());

    if(!body.isNull())
    {
	OS_LOG_DEBUG(FAC_SIP,"IAB: Start Parsing");
	// REVIEW: do not create a new string from the body, use body.str() instead
	std::string amount = aocParser(std::string(body));
	OS_LOG_DEBUG(FAC_SIP,"IAB: Parsed Amount "<< amount);
	if(!amount.empty())
	{
	    // Found amount
	    insertDataToMongoDb(callId,amount);
	    OS_LOG_DEBUG(FAC_SIP,"IAB: Data inserted to MongoDB!");
	}
    }
}


