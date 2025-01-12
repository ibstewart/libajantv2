/* SPDX-License-Identifier: MIT */
/**
	@file		ntv2devicescanner.cpp
	@brief		Implementation of CNTV2DeviceScanner class.
	@copyright	(C) 2004-2022 AJA Video Systems, Inc.
**/

#include "ntv2devicescanner.h"
#include "ntv2devicefeatures.h"
#include "ntv2utils.h"
#include "ajabase/common/common.h"
#include "ajabase/system/lock.h"
#include <sstream>

using namespace std;


#if defined(NTV2_DEPRECATE_17_1)
	//	Abbreviated device info struct
	typedef struct NTV2DeviceInfo
	{
		NTV2DeviceID	deviceID;
		uint64_t		deviceSerialNumber;
		string			deviceIdentifier;
	#if defined(VIRTUAL_DEVICES_SUPPORT)
		bool			isVirtualDevice=false;
		std::string		virtualDeviceName;
		std::string		virtualDeviceID;
	#endif	//	defined(VIRTUAL_DEVICES_SUPPORT)
	} NTV2DeviceInfo;

	typedef vector <NTV2DeviceInfo>		NTV2DeviceInfoList;
	typedef NTV2DeviceInfoList::const_iterator	NTV2DeviceInfoListConstIter;
#else
	bool CNTV2DeviceScanner::IsHexDigit (const char inChr)
	{	static const string sHexDigits("0123456789ABCDEFabcdef");
		return sHexDigits.find(inChr) != string::npos;
	}

	bool CNTV2DeviceScanner::IsDecimalDigit (const char inChr)
	{	static const string sDecDigits("0123456789");
		return sDecDigits.find(inChr) != string::npos;
	}

	bool CNTV2DeviceScanner::IsAlphaNumeric (const char inChr)
	{	static const string sLegalChars("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
		return sLegalChars.find(inChr) != string::npos;
	}

	bool CNTV2DeviceScanner::IsLegalDecimalNumber (const string & inStr, const size_t inMaxLength)
	{
		if (inStr.length() > inMaxLength)
			return false;	//	Too long
		for (size_t ndx(0);  ndx < inStr.size();  ndx++)
			if (!aja::is_decimal_digit(inStr.at(ndx)))
				return false;
		return true;
	}

	uint64_t CNTV2DeviceScanner::IsLegalHexSerialNumber (const string & inStr)	//	0x3236333331375458
	{
		if (inStr.length() < 3)
			return 0ULL;	//	Too small
		string hexStr(inStr); aja::lower(hexStr);
		if (hexStr[0] == '0'  &&  hexStr[1] == 'x')
			hexStr.erase(0, 2);	//	Remove '0x' if present
		if (hexStr.length() > 16)
			return 0ULL;	//	Too big
		for (size_t ndx(0);  ndx < hexStr.size();  ndx++)
			if (!aja::is_hex_digit(hexStr.at(ndx)))
				return 0ULL;	//	Invalid hex digit
		while (hexStr.length() != 16)
			hexStr = '0' + hexStr;	//	prepend another '0'
		istringstream iss(hexStr);
		uint64_t u64(0);
		iss >> hex >> u64;
		return u64;
	}

	bool CNTV2DeviceScanner::IsAlphaNumeric (const string & inStr)
	{
		for (size_t ndx(0);  ndx < inStr.size();  ndx++)
			if (!aja::is_alpha_numeric(inStr.at(ndx)))
				return false;
		return true;
	}
#endif	//	!defined(NTV2_DEPRECATE_17_1)

bool CNTV2DeviceScanner::IsLegalSerialNumber (const string & inStr)
{
	if (inStr.length() != 8  &&  inStr.length() != 9)
		return false;
	return aja::is_alpha_numeric(inStr);
}

static NTV2DeviceInfoList	sDevInfoList;
static AJALock				sDevInfoListLock;

size_t CNTV2DeviceScanner::GetNumDevices (void)
{
	AJAAutoLock tmpLock(&sDevInfoListLock);
	return sDevInfoList.size();
}

#if defined(NTV2_DEPRECATE_17_1)
	void ScanHardware (void)
	{
		AJAAutoLock tmpLock(&sDevInfoListLock);
		sDevInfoList.clear();
		UWord ndx(0);
		do
		{
			CNTV2Card tmpDev(ndx);
			if (!tmpDev.IsOpen())
				break;
			NTV2DeviceInfo info;
			info.deviceID = tmpDev.GetDeviceID();
			info.deviceSerialNumber = tmpDev.GetSerialNumber();
			info.deviceIdentifier = tmpDev.GetDisplayName();
			sDevInfoList.push_back(info);
			ndx++;
		} while (ndx < 16);
	}
#else	//	!defined(NTV2_DEPRECATE_17_1)
CNTV2DeviceScanner::CNTV2DeviceScanner (const bool inScanNow)
{
	if (inScanNow)
		ScanHardware();
}

	#if !defined(NTV2_DEPRECATE_16_3)
		CNTV2DeviceScanner::CNTV2DeviceScanner (bool inScanNow, UWord inDeviceMask)
		{
			(void)inDeviceMask;
			if (inScanNow)
				ScanHardware();
		}
	#endif	//	!defined(NTV2_DEPRECATE_16_3)

NTV2DeviceInfoList	GetDeviceInfoList (void)
{
	AJAAutoLock tmpLock(&sDevInfoListLock);
	return sDevInfoList;
}


void CNTV2DeviceScanner::ScanHardware (void)
{
	AJAAutoLock tmpLock(&sDevInfoListLock);
	sDevInfoList.clear();

	for (UWord boardNum(0);   ;   boardNum++)
	{
		CNTV2Card tmpDev(boardNum);
		if (!tmpDev.IsOpen())
			break;
		const NTV2DeviceID	deviceID (tmpDev.GetDeviceID());

		if (deviceID != DEVICE_ID_NOTFOUND)
		{
			ostringstream	oss;
			NTV2DeviceInfo	info;
			info.deviceIndex		= boardNum;
			info.deviceID			= deviceID;
			info.deviceSerialNumber	= tmpDev.GetSerialNumber();

			oss << ::NTV2DeviceIDToString (deviceID, tmpDev.IsSupported(kDeviceHasMicrophoneInput)) << " - " << boardNum;

			const ULWordSet wgtIDs (tmpDev.GetSupportedItems(kNTV2EnumsID_WidgetID));
			info.deviceIdentifier		= oss.str();
			info.numVidInputs			= tmpDev.GetNumSupported(kDeviceGetNumVideoInputs);
			info.numVidOutputs			= tmpDev.GetNumSupported(kDeviceGetNumVideoOutputs);
			info.numAnlgVidOutputs		= tmpDev.GetNumSupported(kDeviceGetNumAnalogVideoOutputs);
			info.numAnlgVidInputs		= tmpDev.GetNumSupported(kDeviceGetNumAnalogVideoInputs);
			info.numHDMIVidOutputs		= tmpDev.GetNumSupported(kDeviceGetNumHDMIVideoOutputs);
			info.numHDMIVidInputs		= tmpDev.GetNumSupported(kDeviceGetNumHDMIVideoInputs);
			info.numInputConverters		= tmpDev.GetNumSupported(kDeviceGetNumInputConverters);
			info.numOutputConverters	= tmpDev.GetNumSupported(kDeviceGetNumOutputConverters);
			info.numUpConverters		= tmpDev.GetNumSupported(kDeviceGetNumUpConverters);
			info.numDownConverters		= tmpDev.GetNumSupported(kDeviceGetNumDownConverters);
			info.downConverterDelay		= tmpDev.GetNumSupported(kDeviceGetDownConverterDelay);
			info.dvcproHDSupport		= tmpDev.IsSupported(kDeviceCanDoDVCProHD);
			info.qrezSupport			= tmpDev.IsSupported(kDeviceCanDoQREZ);
			info.hdvSupport				= tmpDev.IsSupported(kDeviceCanDoHDV);
			info.quarterExpandSupport	= tmpDev.IsSupported(kDeviceCanDoQuarterExpand);
			info.colorCorrectionSupport	= tmpDev.IsSupported(kDeviceCanDoColorCorrection);
			info.programmableCSCSupport	= tmpDev.IsSupported(kDeviceCanDoProgrammableCSC);
			info.rgbAlphaOutputSupport	= tmpDev.IsSupported(kDeviceCanDoRGBPlusAlphaOut);
			info.breakoutBoxSupport		= tmpDev.IsSupported(kDeviceCanDoBreakoutBox);
			info.vidProcSupport			= tmpDev.IsSupported(kDeviceCanDoVideoProcessing);
			info.dualLinkSupport		= tmpDev.IsSupported(kDeviceCanDoDualLink);
			info.numDMAEngines			= UWord(tmpDev.GetNumSupported(kDeviceGetNumDMAEngines));
			info.pingLED				= tmpDev.GetNumSupported(kDeviceGetPingLED);
			info.has2KSupport			= tmpDev.IsSupported(kDeviceCanDo2KVideo);
			info.has4KSupport			= tmpDev.IsSupported(kDeviceCanDo4KVideo);
			info.has8KSupport			= tmpDev.IsSupported(kDeviceCanDo8KVideo);
			info.has3GLevelConversion   = tmpDev.IsSupported(kDeviceCanDo3GLevelConversion);
			info.isoConvertSupport		= tmpDev.IsSupported(kDeviceCanDoIsoConvert);
			info.rateConvertSupport		= tmpDev.IsSupported(kDeviceCanDoRateConvert);
			info.proResSupport			= tmpDev.IsSupported(kDeviceCanDoProRes);
			info.sdi3GSupport			= wgtIDs.find(NTV2_Wgt3GSDIOut1) != wgtIDs.end();
			info.sdi12GSupport			= tmpDev.IsSupported(kDeviceCanDo12GSDI);
			info.ipSupport				= tmpDev.IsSupported(kDeviceCanDoIP);
			info.biDirectionalSDI		= tmpDev.IsSupported(kDeviceHasBiDirectionalSDI);
			info.ltcInSupport			= tmpDev.GetNumSupported(kDeviceGetNumLTCInputs) > 0;
			info.ltcOutSupport			= tmpDev.GetNumSupported(kDeviceGetNumLTCOutputs) > 0;
			info.ltcInOnRefPort			= tmpDev.IsSupported(kDeviceCanDoLTCInOnRefPort);
			info.stereoOutSupport		= tmpDev.IsSupported(kDeviceCanDoStereoOut);
			info.stereoInSupport		= tmpDev.IsSupported(kDeviceCanDoStereoIn);
			info.multiFormat			= tmpDev.IsSupported(kDeviceCanDoMultiFormat);
			info.numSerialPorts			= tmpDev.GetNumSupported(kDeviceGetNumSerialPorts);
			info.procAmpSupport			= false;
			SetAudioAttributes(info, tmpDev);
			sDevInfoList.push_back(info);
		}
		tmpDev.Close();
	}	//	boardNum loop

#if defined(VIRTUAL_DEVICES_SUPPORT)
	NTV2SerialToVirtualDevices vdMap;
	GetSerialToVirtualDeviceMap(vdMap);
	NTV2DeviceInfoList hwList = GetDeviceInfoList();
	int vdIndex = 100;
	for (auto hwInfo : hwList)
	{
		string hwSN = CNTV2Card::SerialNum64ToString(hwInfo.deviceSerialNumber);
		auto it = vdMap.find(hwSN);
		if (it != vdMap.end())
		{
			std::vector<VirtualDeviceInfo> vdevs = it->second;
			for (const auto& vdev : vdevs)
			{
				hwInfo.deviceIndex = vdIndex++;
				hwInfo.isVirtualDevice = true;
				hwInfo.virtualDeviceID = vdev.vdID;
				hwInfo.virtualDeviceName =  vdev.vdName;
				GetDeviceInfoList().push_back(hwInfo);
			}
		}
	}
#endif	//	defined(VIRTUAL_DEVICES_SUPPORT)
}	//	ScanHardware

bool CNTV2DeviceScanner::DeviceIDPresent (const NTV2DeviceID inDeviceID, const bool inRescan)
{
	AJAAutoLock tmpLock(&sDevInfoListLock);
	if (inRescan)
		ScanHardware();

	for (NTV2DeviceInfoListConstIter iter(sDevInfoList.begin());  iter != sDevInfoList.end();  ++iter)
		if (iter->deviceID == inDeviceID)
			return true;	//	Found!
	return false;	//	Not found

}	//	DeviceIDPresent


bool CNTV2DeviceScanner::GetDeviceInfo (const ULWord inDeviceIndexNumber, NTV2DeviceInfo & outDeviceInfo, const bool inRescan)
{
	AJAAutoLock tmpLock(&sDevInfoListLock);
	if (inRescan)
		ScanHardware();

	if (inDeviceIndexNumber < sDevInfoList.size())
	{
		outDeviceInfo = sDevInfoList[inDeviceIndexNumber];
		return outDeviceInfo.deviceIndex == inDeviceIndexNumber;
	}
	return false;	//	No devices with this index number

}	//	GetDeviceInfo
#endif	//	!defined(NTV2_DEPRECATE_17_1)

bool CNTV2DeviceScanner::GetDeviceAtIndex (const ULWord inDeviceIndexNumber, CNTV2Card & outDevice)
{
	outDevice.Close();
	AJAAutoLock tmpLock(&sDevInfoListLock);
	ScanHardware();
	return size_t(inDeviceIndexNumber) < sDevInfoList.size()
				? outDevice.Open(UWord(inDeviceIndexNumber))
				: false;

}	//	GetDeviceAtIndex


bool CNTV2DeviceScanner::GetFirstDeviceWithID (const NTV2DeviceID inDeviceID, CNTV2Card & outDevice)
{
	outDevice.Close();
	AJAAutoLock tmpLock(&sDevInfoListLock);
	ScanHardware();
	for (size_t ndx(0);  ndx < sDevInfoList.size();  ndx++)
		if (sDevInfoList.at(ndx).deviceID == inDeviceID)
			return outDevice.Open(UWord(ndx));	//	Found!
	return false;	//	Not found

}	//	GetFirstDeviceWithID


bool CNTV2DeviceScanner::GetFirstDeviceWithName (const string & inNameSubString, CNTV2Card & outDevice)
{
	outDevice.Close();
	if (!aja::is_alpha_numeric(inNameSubString))
	{
		if (inNameSubString.find(":") != string::npos)
			return outDevice.Open(inNameSubString);
		return false;
	}

	AJAAutoLock tmpLock(&sDevInfoListLock);
	ScanHardware();
	string	nameSubString(inNameSubString);  aja::lower(nameSubString);
	for (size_t ndx(0);  ndx < sDevInfoList.size();  ndx++)
	{
		string deviceName(sDevInfoList.at(ndx).deviceIdentifier);  aja::lower(deviceName);
		if (deviceName.find(nameSubString) != string::npos)
			return outDevice.Open(UWord(ndx));	//	Found!
	}
	if (nameSubString == "io4kplus")
	{	//	Io4K+ == DNXIV...
		nameSubString = "avid dnxiv";
		for (size_t ndx(0);  ndx < sDevInfoList.size();  ndx++)
		{
			string deviceName(sDevInfoList.at(ndx).deviceIdentifier);  aja::lower(deviceName);
			if (deviceName.find(nameSubString) != string::npos)
				return outDevice.Open(UWord(ndx));	//	Found!
		}
	}
	return false;	//	Not found

}	//	GetFirstDeviceWithName


bool CNTV2DeviceScanner::GetFirstDeviceWithSerial (const string & inSerialStr, CNTV2Card & outDevice)
{
	outDevice.Close();
	AJAAutoLock tmpLock(&sDevInfoListLock);
	ScanHardware();
	string searchSerialStr(inSerialStr);  aja::lower(searchSerialStr);
	for (size_t ndx(0);  ndx < sDevInfoList.size();  ndx++)
	{
		CNTV2Card dev(UWord(ndx+0));
		string serNumStr;
		if (dev.GetSerialNumberString(serNumStr))
		{
			aja::lower(serNumStr);
			if (serNumStr.find(searchSerialStr) != string::npos)
				return outDevice.Open(UWord(ndx));
		}
	}
	return false;
}


bool CNTV2DeviceScanner::GetDeviceWithSerial (const uint64_t inSerialNumber, CNTV2Card & outDevice)
{
	outDevice.Close();
	AJAAutoLock tmpLock(&sDevInfoListLock);
	ScanHardware();
	for (size_t ndx(0);  ndx < sDevInfoList.size();  ndx++)
		if (sDevInfoList.at(ndx).deviceSerialNumber == inSerialNumber)
			return outDevice.Open(UWord(ndx));
	return false;
}


bool CNTV2DeviceScanner::GetFirstDeviceFromArgument (const string & inArgument, CNTV2Card & outDevice)
{
	outDevice.Close();
	if (inArgument.empty())
		return false;

	//	Special case:  'LIST' or '?'  ---  print an enumeration of available devices to stdout, then bail
	AJAAutoLock tmpLock(&sDevInfoListLock);
	ScanHardware();
	string upperArg(inArgument);  aja::upper(upperArg);
	if (upperArg == "LIST" || upperArg == "?")
	{
		if (sDevInfoList.empty())
			cout << "No devices detected" << endl;
		else
			cout << DEC(sDevInfoList.size()) << " available " << (sDevInfoList.size() == 1 ? "device:" : "devices:") << endl;
		for (size_t ndx(0);  ndx < sDevInfoList.size();  ndx++)
		{
			cout << DECN(ndx,2) << " | " << setw(8) << ::NTV2DeviceIDToString(sDevInfoList.at(ndx).deviceID);
			const string serNum(CNTV2Card::SerialNum64ToString(sDevInfoList.at(ndx).deviceSerialNumber));
			if (!serNum.empty())
				cout << " | " << setw(9) << serNum << " | " << HEX0N(sDevInfoList.at(ndx).deviceSerialNumber,8);
			cout << endl;
		}
#if defined(VIRTUAL_DEVICES_SUPPORT)
		if (iter != sDevInfoList.end())
		{
			cout << "*** Virtual Devices ***" << endl;
			while (iter != sDevInfoList.end())
			{
				const string serNum(CNTV2Card::SerialNum64ToString(iter->deviceSerialNumber));
				cout << DECN(iter->deviceIndex,2) << " | " << setw(15) << iter->virtualDeviceName;// NTV2DeviceIDToString(iter->deviceID);
				cout << " | " << iter->virtualDeviceID;
				cout << " (" << NTV2DeviceIDToString(iter->deviceID); 
				if (!serNum.empty())
					cout << " " << serNum;
				cout << ")" << endl;
				iter++;
			}
		}
#endif	//	defined(VIRTUAL_DEVICES_SUPPORT)
		return false;
	}

#if defined(VIRTUAL_DEVICES_SUPPORT)
	// See if any virtual devices are being referenced by their Index or VD Name. 
	// If so, convert the argument to the RPC URL and open it.
	string cp2ConfigPath;
	GetCP2ConfigPath(cp2ConfigPath);  
	std::ifstream cfgJsonfile(cp2ConfigPath);  //VDTODO, error handling
	for (NTV2DeviceInfoListConstIter iter(sDevInfoList.begin());  iter != sDevInfoList.end();  ++iter)
	{
		if (iter->isVirtualDevice)
		{
			if ( (std::to_string(iter->deviceIndex) == inArgument ) ||
				iter->virtualDeviceName == inArgument ||
				iter->virtualDeviceID == inArgument)
			{
				string inVDSpec = "ntv2virtualdev://localhost/?CP2ConfigPath=" + cp2ConfigPath +
						  "&DeviceSN=" +  CNTV2Card::SerialNum64ToString(iter->deviceSerialNumber) + 
						  "&vdid=" + iter->virtualDeviceID + 
						  "&verbose";
				return outDevice.Open(inVDSpec);
			}
		}
	}

#endif	//	defined(VIRTUAL_DEVICES_SUPPORT)
	return outDevice.Open(inArgument);

}	//	GetFirstDeviceFromArgument


string CNTV2DeviceScanner::GetDeviceRefName (CNTV2Card & inDevice)
{	//	Name that will find given device via CNTV2DeviceScanner::GetFirstDeviceFromArgument
	if (!inDevice.IsOpen())
		return string();
	//	Nub address 1st...
	if (!inDevice.GetHostName().empty()  &&  inDevice.IsRemote())
		return inDevice.GetHostName();	//	Nub host/device

	//	Serial number 2nd...
	string str;
	if (inDevice.GetSerialNumberString(str))
		return str;

	//	Model name 3rd...
	str = ::NTV2DeviceIDToString(inDevice.GetDeviceID(), false);
	if (!str.empty() &&  str != "???")
		return str;

	//	Index number last...
	ostringstream oss;  oss << DEC(inDevice.GetIndexNumber());
	return oss.str();
}


#if !defined(NTV2_DEPRECATE_17_1)
ostream &	operator << (ostream & inOutStr, const NTV2DeviceInfoList & inList)
{
	for (NTV2DeviceInfoListConstIter iter(inList.begin());  iter != inList.end();  ++iter)
		inOutStr << " " << *iter;
	return inOutStr;

}	//	NTV2DeviceInfoList ostream operator <<


bool NTV2DeviceInfo::operator == (const NTV2DeviceInfo & second) const
{
	const NTV2DeviceInfo &	first	(*this);
	size_t					diffs	(0);

	//	'memcmp' would be simpler, but because NTV2DeviceInfo has no constructor, the unfilled bytes in
	//	its "boardIdentifier" field are indeterminate, making it worthless for accurate comparisons.
	//	"boardSerialNumber" and boardNumber are the only required comparisons, but I also check boardType,
	//	boardID, and pciSlot for good measure...
	if (first.deviceID							!=	second.deviceID)						diffs++;
	if (first.deviceIndex						!=	second.deviceIndex)						diffs++;
	if (first.deviceSerialNumber				!=	second.deviceSerialNumber)				diffs++;
	if (first.pciSlot							!=	second.pciSlot)							diffs++;
	
	// Needs to be fixed now that deviceIdentifier is a std::string
	//#if defined (AJA_DEBUG)
	//	if (::strncmp (first.deviceIdentifier.c_str (), second.deviceIdentifier.c_str (), first.deviceIdentifier.length ())))	diffs++;
	//	if (diffs)
	//		{cout << "## DEBUG:  " << diffs << " diff(s):" << endl << "#### first ####" << endl << first << "#### second ####" << endl << second << endl;}
	//#endif	//	AJA_DEBUG

	return diffs ? false : true;

}	//	equality operator


bool CNTV2DeviceScanner::CompareDeviceInfoLists (const NTV2DeviceInfoList & inOldList,
											const NTV2DeviceInfoList & inNewList,
											NTV2DeviceInfoList & outBoardsAdded,
											NTV2DeviceInfoList & outBoardsRemoved)
{
	NTV2DeviceInfoListConstIter	oldIter	(inOldList.begin ());
	NTV2DeviceInfoListConstIter	newIter	(inNewList.begin ());

	outBoardsAdded.clear ();
	outBoardsRemoved.clear ();

	while (true)
	{
		if (oldIter == inOldList.end () && newIter == inNewList.end ())
			break;	//	Done -- exit

		if (oldIter != inOldList.end () && newIter != inNewList.end ())
		{
			const NTV2DeviceInfo &  oldInfo (*oldIter),  newInfo (*newIter);

			if (oldInfo != newInfo)
			{
				//	Out with the old...
				outBoardsRemoved.push_back (oldInfo);

				//	In with the new...
				if (newInfo.deviceID && newInfo.deviceID != NTV2DeviceID(0xFFFFFFFF))
					outBoardsAdded.push_back (newInfo);
			}	//	if mismatch

			++oldIter;
			++newIter;
			continue;	//	Move along

		}	//	if both valid

		if (oldIter != inOldList.end () && newIter == inNewList.end ())
		{
			outBoardsRemoved.push_back (*oldIter);
			++oldIter;
			continue;	//	Move along
		}	//	old is valid, new is not valid

		if (oldIter == inOldList.end () && newIter != inNewList.end ())
		{
			if (newIter->deviceID && newIter->deviceID != NTV2DeviceID(0xFFFFFFFF))
				outBoardsAdded.push_back (*newIter);
			++newIter;
			continue;	//	Move along
		}	//	old is not valid, new is valid

		NTV2_ASSERT(false && "should never get here");

	}	//	loop til break

	//	Return 'true' if there were any changes...
	return !outBoardsAdded.empty () || !outBoardsRemoved.empty ();

}	//	CompareDeviceInfoLists


ostream &	operator << (ostream & inOutStr, const NTV2AudioSampleRateList & inList)
{
	for (NTV2AudioSampleRateListConstIter iter (inList.begin ()); iter != inList.end (); ++iter)
		inOutStr << " " << *iter;

	return inOutStr;
}


ostream &	operator << (ostream & inOutStr, const NTV2AudioChannelsPerFrameList & inList)
{
	for (NTV2AudioChannelsPerFrameListConstIter iter (inList.begin ());  iter != inList.end ();  ++iter)
		inOutStr << " " << *iter;

	return inOutStr;
}


ostream &	operator << (ostream & inOutStr, const NTV2AudioSourceList & inList)
{
	for (NTV2AudioSourceListConstIter iter(inList.begin());  iter != inList.end();  ++iter)
		switch (*iter)	//	AudioSourceEnum
		{
			case kSourceSDI:	return inOutStr << " SDI";
			case kSourceAES:	return inOutStr << " AES";
			case kSourceADAT:	return inOutStr << " ADAT";
			case kSourceAnalog:	return inOutStr << " Analog";
			case kSourceNone:	return inOutStr << " None";
			case kSourceAll:	return inOutStr << " All";
		}
	return inOutStr << " ???";
}


ostream &	operator << (ostream & inOutStr, const NTV2AudioBitsPerSampleList & inList)
{
	for (NTV2AudioBitsPerSampleListConstIter iter (inList.begin ());  iter != inList.end ();  ++iter)
		inOutStr << " " << *iter;

	return inOutStr;
}


ostream &	operator << (ostream & inOutStr, const NTV2DeviceInfo & inInfo)
{
	inOutStr	<< "Device Info for '" << inInfo.deviceIdentifier << "'" << endl
				<< "            Device Index Number: " << inInfo.deviceIndex << endl
				<< "                      Device ID: 0x" << hex << inInfo.deviceID << dec << endl
				<< "                  Serial Number: 0x" << hex << inInfo.deviceSerialNumber << dec << endl
				<< "                       PCI Slot: 0x" << hex << inInfo.pciSlot << dec << endl
				<< "                   Video Inputs: " << inInfo.numVidInputs << endl
				<< "                  Video Outputs: " << inInfo.numVidOutputs << endl
				#if defined (_DEBUG)
					<< "            Analog Video Inputs: " << inInfo.numAnlgVidInputs << endl
					<< "           Analog Video Outputs: " << inInfo.numAnlgVidOutputs << endl
					<< "              HDMI Video Inputs: " << inInfo.numHDMIVidInputs << endl
					<< "             HDMI Video Outputs: " << inInfo.numHDMIVidOutputs << endl
					<< "               Input Converters: " << inInfo.numInputConverters << endl
					<< "              Output Converters: " << inInfo.numOutputConverters << endl
					<< "                  Up Converters: " << inInfo.numUpConverters << endl
					<< "                Down Converters: " << inInfo.numDownConverters << endl
					<< "           Down Converter Delay: " << inInfo.downConverterDelay << endl
					<< "                       DVCProHD: " << (inInfo.dvcproHDSupport ? "Y" : "N") << endl
					<< "                           Qrez: " << (inInfo.qrezSupport ? "Y" : "N") << endl
					<< "                            HDV: " << (inInfo.hdvSupport ? "Y" : "N") << endl
					<< "                 Quarter Expand: " << (inInfo.quarterExpandSupport ? "Y" : "N") << endl
					<< "                    ISO Convert: " << (inInfo.isoConvertSupport ? "Y" : "N") << endl
					<< "                   Rate Convert: " << (inInfo.rateConvertSupport ? "Y" : "N") << endl
					<< "                        VidProc: " << (inInfo.vidProcSupport ? "Y" : "N") << endl
					<< "                      Dual-Link: " << (inInfo.dualLinkSupport ? "Y" : "N") << endl
					<< "               Color-Correction: " << (inInfo.colorCorrectionSupport ? "Y" : "N") << endl
					<< "               Programmable CSC: " << (inInfo.programmableCSCSupport ? "Y" : "N") << endl
					<< "               RGB Alpha Output: " << (inInfo.rgbAlphaOutputSupport ? "Y" : "N") << endl
					<< "                   Breakout Box: " << (inInfo.breakoutBoxSupport ? "Y" : "N") << endl
					<< "                        ProcAmp: " << (inInfo.procAmpSupport ? "Y" : "N") << endl
					<< "                             2K: " << (inInfo.has2KSupport ? "Y" : "N") << endl
					<< "                             4K: " << (inInfo.has4KSupport ? "Y" : "N") << endl
					<< "                             8K: " << (inInfo.has8KSupport ? "Y" : "N") << endl
                    << "            3G Level Conversion: " << (inInfo.has3GLevelConversion ? "Y" : "N") << endl
					<< "                         ProRes: " << (inInfo.proResSupport ? "Y" : "N") << endl
					<< "                         SDI 3G: " << (inInfo.sdi3GSupport ? "Y" : "N") << endl
					<< "                        SDI 12G: " << (inInfo.sdi12GSupport ? "Y" : "N") << endl
					<< "                             IP: " << (inInfo.ipSupport ? "Y" : "N") << endl
					<< "             SDI Bi-Directional: " << (inInfo.biDirectionalSDI ? "Y" : "N") << endl
					<< "                         LTC In: " << (inInfo.ltcInSupport ? "Y" : "N") << endl
					<< "                        LTC Out: " << (inInfo.ltcOutSupport ? "Y" : "N") << endl
					<< "             LTC In on Ref Port: " << (inInfo.ltcInOnRefPort ? "Y" : "N") << endl
					<< "                     Stereo Out: " << (inInfo.stereoOutSupport ? "Y" : "N") << endl
					<< "                      Stereo In: " << (inInfo.stereoInSupport ? "Y" : "N") << endl
					<< "             Audio Sample Rates: " << inInfo.audioSampleRateList << endl
					<< "           AudioNumChannelsList: " << inInfo.audioNumChannelsList << endl
					<< "         AudioBitsPerSampleList: " << inInfo.audioBitsPerSampleList << endl
					<< "              AudioInSourceList: " << inInfo.audioInSourceList << endl
					<< "             AudioOutSourceList: " << inInfo.audioOutSourceList << endl
					<< "                  Audio Streams: " << inInfo.numAudioStreams << endl
					<< "    Analog Audio Input Channels: " << inInfo.numAnalogAudioInputChannels << endl
					<< "   Analog Audio Output Channels: " << inInfo.numAnalogAudioOutputChannels << endl
					<< "       AES Audio Input Channels: " << inInfo.numAESAudioInputChannels << endl
					<< "      AES Audio Output Channels: " << inInfo.numAESAudioOutputChannels << endl
					<< "  Embedded Audio Input Channels: " << inInfo.numEmbeddedAudioInputChannels << endl
					<< " Embedded Audio Output Channels: " << inInfo.numEmbeddedAudioOutputChannels << endl
					<< "      HDMI Audio Input Channels: " << inInfo.numHDMIAudioInputChannels << endl
					<< "     HDMI Audio Output Channels: " << inInfo.numHDMIAudioOutputChannels << endl
					<< "                    DMA Engines: " << inInfo.numDMAEngines << endl
					<< "                   Serial Ports: " << inInfo.numSerialPorts << endl
				#endif	//	AJA_DEBUG
				<< "";

	return inOutStr;

}	//	NTV2DeviceInfo ostream operator <<


ostream &	operator << (ostream & inOutStr, const NTV2AudioPhysicalFormat & inFormat)
{
	inOutStr	<< "AudioPhysicalFormat:" << endl
				<< "    boardNumber: " << inFormat.boardNumber << endl
				<< "     sampleRate: " << inFormat.sampleRate << endl
				<< "    numChannels: " << inFormat.numChannels << endl
				<< "  bitsPerSample: " << inFormat.bitsPerSample << endl
	#if defined (DEBUG) || defined (AJA_DEBUG)
				<< "       sourceIn: 0x" << hex << inFormat.sourceIn << dec << endl
				<< "      sourceOut: 0x" << hex << inFormat.sourceOut << dec << endl
	#endif	//	DEBUG or AJA_DEBUG
				;

	return inOutStr;

}	//	AudioPhysicalFormat ostream operator <<


std::ostream &	operator << (std::ostream & inOutStr, const NTV2AudioPhysicalFormatList & inList)
{
	for (NTV2AudioPhysicalFormatListConstIter iter (inList.begin ()); iter != inList.end (); ++iter)
		inOutStr << *iter;

	return inOutStr;

}	//	AudioPhysicalFormatList ostream operator <<


// Private methods

void CNTV2DeviceScanner::SetAudioAttributes (NTV2DeviceInfo & info, CNTV2Card & inBoard)
{
	//	Start with empty lists...
	info.audioSampleRateList.clear();
	info.audioNumChannelsList.clear();
	info.audioBitsPerSampleList.clear();
	info.audioInSourceList.clear();
	info.audioOutSourceList.clear();


	if (inBoard.GetNumSupported(kDeviceGetNumAudioSystems))
	{
		ULWord audioControl;
		inBoard.ReadRegister(kRegAud1Control, audioControl);

		//audioSampleRateList
		info.audioSampleRateList.push_back(k48KHzSampleRate);
		if (inBoard.IsSupported(kDeviceCanDoAudio96K))
			info.audioSampleRateList.push_back(k96KHzSampleRate);

		//audioBitsPerSampleList
		info.audioBitsPerSampleList.push_back(k32bitsPerSample);

		//audioInSourceList
		info.audioInSourceList.push_back(kSourceSDI);
		if (audioControl & BIT(21))
			info.audioInSourceList.push_back(kSourceAES);
		if (inBoard.IsSupported(kDeviceCanDoAnalogAudio))
			info.audioInSourceList.push_back(kSourceAnalog);

		//audioOutSourceList
		info.audioOutSourceList.push_back(kSourceAll);

		//audioNumChannelsList
		if (inBoard.IsSupported(kDeviceCanDoAudio2Channels))
			info.audioNumChannelsList.push_back(kNumAudioChannels2);
		if (inBoard.IsSupported(kDeviceCanDoAudio6Channels))
			info.audioNumChannelsList.push_back(kNumAudioChannels6);
		if (inBoard.IsSupported(kDeviceCanDoAudio8Channels))
			info.audioNumChannelsList.push_back(kNumAudioChannels8);

		info.numAudioStreams = inBoard.GetNumSupported(kDeviceGetNumAudioSystems);
	}

	info.numAnalogAudioInputChannels	= inBoard.GetNumSupported(kDeviceGetNumAnalogAudioInputChannels);
	info.numAESAudioInputChannels		= inBoard.GetNumSupported(kDeviceGetNumAESAudioInputChannels);
	info.numEmbeddedAudioInputChannels	= inBoard.GetNumSupported(kDeviceGetNumEmbeddedAudioInputChannels);
	info.numHDMIAudioInputChannels		= inBoard.GetNumSupported(kDeviceGetNumHDMIAudioInputChannels);
	info.numAnalogAudioOutputChannels	= inBoard.GetNumSupported(kDeviceGetNumAnalogAudioOutputChannels);
	info.numAESAudioOutputChannels		= inBoard.GetNumSupported(kDeviceGetNumAESAudioOutputChannels);
	info.numEmbeddedAudioOutputChannels	= inBoard.GetNumSupported(kDeviceGetNumEmbeddedAudioOutputChannels);
	info.numHDMIAudioOutputChannels		= inBoard.GetNumSupported(kDeviceGetNumHDMIAudioOutputChannels);

}	//	SetAudioAttributes


#if defined(VIRTUAL_DEVICES_SUPPORT)
bool CNTV2DeviceScanner::GetSerialToVirtualDeviceMap (NTV2SerialToVirtualDevices & outSerialToVirtualDevMap)
{
	string cp2ConfigPath;
	GetCP2ConfigPath(cp2ConfigPath);  
	std::ifstream cfgJsonfile(cp2ConfigPath); 
	json cp2Json;
	if (cfgJsonfile.is_open())
		//VDTODO: Send to debug sucessful open
		cp2Json = json::parse(cfgJsonfile);   //VDTODO  handle any parse_error exception, send to error
	else
		//VDTODO: Send to debug fail to open
		return false;
	cfgJsonfile.close();

	for (const auto& hwdev : cp2Json["v2"]["deviceConfigList"]) 
	{
		std::vector<VirtualDeviceInfo> vdevs;
		json hwdevice = hwdev;
		for (const auto& vdev : hwdevice["virtualDevices"]) 
		{
			VirtualDeviceInfo newVDev;
			newVDev.vdID = vdev["id"];
			newVDev.vdName = vdev["name"];
			vdevs.push_back(newVDev);
		}
		if (!vdevs.empty())
			outSerialToVirtualDevMap[hwdev["serial"]] = vdevs;
	}

	return true;
}

bool CNTV2DeviceScanner::GetCP2ConfigPath(string & outCP2ConfigPath)
{
	AJASystemInfo info;
	info.GetValue(AJA_SystemInfoTag_Path_PersistenceStoreUser,outCP2ConfigPath);
	outCP2ConfigPath = outCP2ConfigPath + "aja/controlpanelConfigPrimary.json";
	return true;
}
#endif	//	defined(VIRTUAL_DEVICES_SUPPORT)
#endif	//	!defined(NTV2_DEPRECATE_17_1)
