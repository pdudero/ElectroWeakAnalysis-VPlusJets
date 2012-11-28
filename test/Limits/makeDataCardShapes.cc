/*
makeLimitFile

Some important constants are set at the top of the file.
*/
#include <assert.h>
#include <iostream>
#include <iomanip>
#include <set>
#include <vector>
#include <math.h>
#include <climits>
#include <stdio.h>
#include <stdlib.h>
#include "TFile.h"
#include "TH1.h"
#include "TF1.h"
#include "TDirectory.h"
#include "TObjString.h"
#include "TObjArray.h"
#include "TKey.h"
#include "TClass.h"

using namespace std;

#include "dcutils.C"

#include "hwwinputs.h"
//#include "hwwinputs_rectcut2.h"
#include "hwwutils.C"
//#include "mjjinputs.h"

//================================================================================

int
tokNameGetInfo(const TString& hname,
	       TString& procname,
	       TString& systname,
	       int&     ichan,
	       int&     imass,
	       bool&    isinterp)
{
  char name[128];

  strcpy(name,hname.Data());

  // use "strtok" because I can stop tokenizing in mid-string
  //
  char *tok = strtok(name,"_");  assert(tok);

  procname = TString(tok);

  TString channame;
  if (!procname.CompareTo("data")) {
    strtok(NULL,"_"); // skip "obs"
  }
  tok = strtok(NULL,"_");  assert(tok);
  channame = TString(tok);

  for (ichan=0; ichan<NUMCHAN; ichan++)
    if (!strcmp(channames[ichan],channame.Data())) break;
  if (ichan>=NUMCHAN) {
    cerr << "channel " << channame << "not found, skipping" << endl;
    return 0;
  }

  // if there's a mass embedded in the name it has to be now:
  int massgev=0;
  if (hname.Contains("Mass")) {
    tok = strtok(NULL,"_");    assert(tok); assert(!strcmp(tok,"Mass"));
    tok = strtok(NULL,"_");    assert(tok); massgev = atoi(tok);

    for (imass=0;massgev!=masspts[imass] && imass<NUMMASSPTS; imass++) ;
    if (imass<NUMMASSPTS) {
      isinterp=false;
    } else {
      for (imass=0;massgev!=interpolatedmasspts[imass]; imass++) 
	if (interpolatedmasspts[imass] < 0) {
	  cerr << "mass point M=" << massgev << " GeV not found, skipping" << endl;
	  return 0;
	}
      isinterp=true;
    }
  }

  tok = strtok(NULL,""); // the rest of the string may be systematic

  if (tok) {
    TString test(tok);
    if (test.EndsWith("Up")) { // This is a histogram with a systematic applied
      systname = test.Chop().Chop();
    }
  }

  cout<<"Read process="<<setw(10)<<procname<<", channel="<<channame<<", mass="<<massgev;
  if (isinterp) cout << " (interpolated)";
  if (systname.Length()) cout << ", systname = " << systname;

  return 1;
}                                                                // tokNameGetInfo

//================================================================================

CardData_t 
makeNewCard(TH1           *inhist,
	    const TString& procname,
	    const TString& systname,
	    const int      ichanref
	    )
{
  CardData_t card;

  card.nbackproc = 0;
  card.nsigproc  = 0;

  cout << "\tmake new card ";

  ProcData_t pd;

  pair<TString,double> channel;

  TString channame = channames[ichanref];
  pd.name          = procname;
  channel.first    = channame;
  channel.second   = inhist->Integral(); // the yield for this channel

  pd.channels.insert(channel);

  if( procname.Contains("data") ) {
    card.data = pd; // done.
  } else {

    if( procname.Contains("ignal")
#ifdef ISHWW
	|| (procname.Contains("ggH") ||
	    procname.Contains("qq") )
#endif //ISHWW
	) {                              // signal

      assert(!systname.Length());     // don't expect to encounter shape systematic first.

      card.nsigproc++;
      pd.procindex = 1-card.nsigproc; /* process index for signal processes required to be distinct,
					 as well as 0 or negative */
    } else {                                                        //background
      card.nbackproc++;
      pd.procindex = card.nbackproc;
    }
      
    card.processes.push_back(pd);
    card.pname2index[procname]= 0;

  } // else not data

  card.channels.insert(channame);

  return card;
}                                                                   // makeNewCard

//================================================================================

void
addToCard(CardData_t&    card,
	  TH1           *inhist,   // input histogram
	  const TString& procname, // process name 
	  const TString& systname, // name of (shape) systematic applied
	  const int      ichanref, // channel reference index
	  const int      ichan,    // channel index
	  const int      nchan     // number of channels in card
	  )
{
  cout << "\tadd to card ";

  // With each input histogram one must:
  // 1. If a new process, update the process info in the card: "card.processes", "card.pname2index",
  // 2. Update new channel information "card.processes[procindex].channels" (channel name/rate pairs)
  // 3. Update the systematics information as necessary:
  //    if systname.length, add shape systematic information to
  //    card.systematics (if new systematic), card.processes[procindex].systrates[systname][ichan]
  // 
  pair<TString,double> channel;

  const pair<double,double> zeropair(0.0,0.0);

  TString channame(channames[ichanref]);

  card.channels.insert(channame);

  channel.first  = channame;
  channel.second = inhist->Integral();
  
  if( procname.Contains("data") )         // data observation

    if (!card.data.name.Length()) {             // first channel of data encountered
      ProcData_t pd;
      pd.name = procname;
      pd.channels.insert(channel);
      card.data = pd;
    }else{                                      // another channel for data, possibly
      assert(!card.data.name.CompareTo(procname)); // must be the same
      card.data.channels.insert(channel);
    }

  else if( procname.Contains("ignal")
#ifdef ISHWW
	    || (procname.Contains("ggH") ||
		procname.Contains("qq") )
#endif //ISHWW
	   ) { // signal expectation


    map<TString,int>::iterator pit;
    pit = card.pname2index.find(procname);
    if (pit == card.pname2index.end()) {    // new signal process, first channel encountered
      card.nsigproc++;

      assert(!systname.Length());     // don't expect to encounter shape systematic first

      ProcData_t pd;
      pd.name      = procname;
      pd.procindex = 1-card.nsigproc; /* process index for signal processes required to be distinct,
					 as well as 0 or negative */

      pd.channels.insert(channel);

      // put new signal in front, have to adjust the mapped indices for all the rest.
      // This maintains the proper ordering of processes in the datacard (for LandS)
      //
      for (pit = card.pname2index.begin(); pit != card.pname2index.end(); pit++)
	pit->second++;

      card.processes.push_front(pd);

      card.pname2index[pd.name] = 0;

    } else {                        // another channel for signal, or a shape systematic

      ProcData_t& pd = card.processes[pit->second];
      assert(pd.name.Length());             // process should exist here
      assert(!pd.name.CompareTo(procname)); // must be the same

      if (systname.Length()) {
#ifdef ISHWW
	if (procname.Contains("ggH")) {
	  // In this case particularly we do *not* pay attention to the systematics name, but
	  // assume this is the common interference shape systematic for ggH signal
	  //
	  const pair<double,double> onepair(0.0,1.0);

	  pd.systrates["interf_ggH"].resize(nchan,onepair); /* shape-based systematic,
							       assume all channels CORRELATED,
							       so all channels get factor 1.0, */
	}
#else
	card.systematics[systname] = "shape";

	pd.systrates[systname].resize(nchan,zeropair); /* shape-based systematic,
							  assume all channels uncorrelated,
							  so all channels get factor 0.0, */
	pd.systrates[systname][ichan].second = 1.0;     // except the current channel
#endif

      } else {

	pd.channels.insert(channel);

      } // nominal signal histo
    } // signal histo for another channel, or systematic

  } else {                               // background process

    map<TString,int>::iterator pit;
    pit = card.pname2index.find(procname);
    if (pit == card.pname2index.end()) {    // first channel of this background process encountered
      card.nbackproc++;

      ProcData_t pd;
      pd.name = procname;
      pd.procindex = card.nbackproc;
      pd.channels.insert(channel);

      card.pname2index[pd.name] = (int)card.processes.size();
      card.processes.push_back(pd);

    } else {                                /* either another channel, or a shape
					       systematic for an existing channel */
      ProcData_t& pd = card.processes[pit->second];

      if (systname.Length()) { // a shape-based limit, all channels get a factor of 0.0
	card.systematics[systname] = "shape";
	pd.systrates[systname].resize(nchan,zeropair);
	pd.systrates[systname][ichan].second = 1.0; // except the current channel
      }
      else {
	assert(pd.name.Length());             // process should exist here
	assert(!pd.name.CompareTo(procname)); // and be named the same
	pd.channels.insert(channel);
      }
    }
  }
}                                                                     // addToCard

//================================================================================

void addSystematics(int massgev,
		    CardData_t& card,
		    const vector<int>& channellist)
{
  int nchan = (int)card.channels.size();

  // per-channel systematics:
  for (std::set<TString>::iterator it=card.channels.begin(); it!=card.channels.end(); it++) {
    TString channame = *it;

    // all cards have these by default; these are the non-shape-based systematics
    //
    TString bckgrdsyst = Form("CMS_%s_norm_back_%dTeV",channame.Data(),beamcomenergytev); // channel-dependent
    TString signalsyst = Form("CMS_%s_eff_sig_%dTeV",channame.Data(),beamcomenergytev);   // channel-dependent

    char elormu = channame[ELORMUCHAR];
    //TString trigsyst   = "CMS_trigger_"+TString(elormu);
    TString leptsyst   = "CMS_eff_"+TString(elormu);

    card.systematics[bckgrdsyst]   = "lnN";
#ifdef ISHWW
    card.systematics[signalsyst]   = "lnN";
#endif //ISHWW
    //card.systematics[trigsyst]     = "lnN"; // common across same-flavor channels
    card.systematics[leptsyst]     = "lnN"; // common across same-flavor channels
  }

  // common systematics
  card.systematics["lumi_8TeV"]    = "lnN"; // common across channels

#ifdef ISHWW
  card.systematics["pdf_gg"]       = "lnN"; // common across channels for ggF process
#ifdef SEVENTEV
  card.systematics["pdf_qqbar"]    = "lnN"; // common across channels for VBF process
#endif
  card.systematics["QCDscale_ggH"]    = "lnN"; // common across channels for ggF process 2j bin
  card.systematics["QCDscale_ggH1in"] = "lnN"; // common across channels for ggF process 2j/3j bins
  card.systematics["QCDscale_ggH2in"] = "lnN"; // common across channels for ggF process 3j bin
  card.systematics["UEPS"]            = "lnN"; // common across channels for ggF process 2j/3j bins
  card.systematics["interf_ggH"]      = "shape"; // common across channels for ggF process 2j/3j bins
#ifdef SEVENTEV
  card.systematics["QCDscale_qqH"]  = "lnN"; // common across channels for VBF process
#endif
#endif // ISHWW

  std::map<TString,int>::const_iterator it;
  for (it=card.pname2index.begin(); it != card.pname2index.end(); it++) {
    TString procname = it->first;
    int index = it->second;

    ProcData_t& pd = card.processes[index];

    if( procname.Contains("data") ) return;

    if( procname.Contains("ignal")
#ifdef ISHWW
	|| (procname.Contains("ggH") ||
	    procname.Contains("qq") )
#endif //ISHWW
	) {                              // signal

      // insert signal lumi and xsec systematics now
#ifdef ISHWW

      pair<double,double> lumipair(0.0, 1+siglumiunc);

      pd.systrates["lumi_8TeV"].resize(nchan,lumipair);

      // down/up pairs to put in card
      pair<double,double> pdfunc,scaleunc0,scaleunc1,scaleunc2,scaleunc3,ueps0,ueps1; 
    
      makeTheoretUncert4Sig(massgev,procname,pdfunc,scaleunc0,scaleunc1,scaleunc2,scaleunc3,ueps0,ueps1);

      if (procname.Contains("qq") ) { // VBF process
	
	pd.systrates["pdf_qqbar"].resize(nchan,pdfunc);
	pd.systrates["QCDscale_qqH"].resize(nchan,scaleunc0);
      
      } else { // default gg fusion

	pd.systrates["pdf_gg"].resize(nchan,pdfunc);
	pd.systrates["QCDscale_ggH"].resize(nchan,zeropair);
	pd.systrates["QCDscale_ggH1in"].resize(nchan,zeropair);
	pd.systrates["QCDscale_ggH2in"].resize(nchan,zeropair);
	pd.systrates["UEPS"].resize(nchan,zeropair);
      }
#endif //ISHWW

      int ichan=-1;
      for (int ichanref=0; ichanref<NUMCHAN; ichanref++) { //  depends on channames being lexically ordered!

	if (card.channels.find(channames[ichanref]) == card.channels.end()) {
	  cout << "...skipping."  << endl;
	  continue;  // check if on the list of channels to be done
	}
      
	ichan++;

	char elormu = channames[ichanref][ELORMUCHAR];
	//TString trigsyst   = "CMS_trigger_"+TString(elormu);
	TString leptsyst   = "CMS_eff_"+TString(elormu);

	if (ichanref & 1) { // odd channel, 3jet bin
	  pd.systrates["QCDscale_ggH1in"][ichan] = scaleunc2;
	  pd.systrates["QCDscale_ggH2in"][ichan] = scaleunc3;
	  pd.systrates["UEPS"][ichan]            = ueps1;
	} else { // even channel, 2jet bin
	  pd.systrates["QCDscale_ggH"][ichan]    = scaleunc0;
	  pd.systrates["QCDscale_ggH1in"][ichan] = scaleunc1;
	  pd.systrates["UEPS"][ichan]            = ueps0;
	}

	//pd.systrates[trigsyst].resize(nchan,zeropair);
	//pd.systrates[trigsyst][ichan].second = 1+sigtrigeffunc;
	if (!pd.systrates[leptsyst].size())
	  pd.systrates[leptsyst].resize(nchan,zeropair);
	pd.systrates[leptsyst][ichan].second = 1+sqrt(siglepteffunc*siglepteffunc + sigtrigeffunc*sigtrigeffunc);

#ifdef ISHWW
	TString signalsyst = Form("CMS_%s_eff_sig_%dTeV",  channames[ichanref],beamcomenergytev);
	if (!pd.systrates[signalsyst].size())
	  pd.systrates[signalsyst].resize(nchan,zeropair);
	pd.systrates[signalsyst][ichan].second =
#ifdef SEVENTEV
	  1.0 + (massgev < 500 ? sigselefferrpctlomass : sigselefferrpcthimass)/100.;
#else
	  1.0 + (sigselefferrpct8tev)/100.;
#endif
#endif //ISHWW

      } // channel loop

    } else {                                                        //background
    
      int imass2use = findMasspt2use(massgev);

      int ichan=-1;
      for (int ichanref=0; ichanref<NUMCHAN; ichanref++) { //  depends on channames being lexically ordered!

	if (card.channels.find(channames[ichanref]) == card.channels.end()) {
	  cout << "...skipping."  << endl;
	  continue;  // check if on the list of channels to be done
	}
      
	ichan++;

	TString bckgrdsyst = Form("CMS_%s_norm_back_%dTeV",channames[ichanref],beamcomenergytev);
    
	if ( pd.name.Contains("kgr") ) {
	  if (!pd.systrates[bckgrdsyst].size())
	    pd.systrates[bckgrdsyst].resize(nchan,zeropair);
	  int index = imass2use*NUMCHAN+ichanref;
	  //cout << imass2use << " " << ichanref << " " << ichan << " " << index << endl;
	  pd.systrates[bckgrdsyst][ichan].second = backnormerr[index];
	}
      } // channel loop

    } // if is background

  } // process loop

}                                                                // addSystematics

//================================================================================

void
makeDataCardContent(TFile *fp, map<int,CardData_t>& m_cards,
		    const vector<int>& channellist)
{
  TIter nextkey( gDirectory->GetListOfKeys() );
  TKey *key;

  cout << "Looking for channels ";
  for (size_t i=0; i<channellist.size(); i++) {
    cout << channames[channellist[i]] << " ";
  }
  cout << endl;

  // loop through objects in the input root file and find histograms
  // that are shape inputs into the limit setting data card
  //
  while ( (key = (TKey*)nextkey())) {

    TObject *obj = key->ReadObj();

    if ( !obj->IsA()->InheritsFrom( "TH1" ) )
      continue;

    TH1 *h1 = (TH1*)obj;
    TString hname(h1->GetName());

    if (hname.EndsWith("Down")) continue; // skip the "Down"s, expect a matching "Up"

    // tokenize the histo name to determine
    // 1. process name
    // 2. channel name
    // 3. mass point
    // 4. any systematic applied
    //
    TString procname(""),systname(""),channname("");
    int ichanref=0,imass=0;
    bool isinterp;

    if (!tokNameGetInfo(hname, procname,systname,ichanref,imass,isinterp)) {
      cout << "...skipping."  << endl;
      continue;
    }

    size_t ichan;
    for (ichan=0; ichan<channellist.size(); ichan++)
      if (ichanref == channellist[ichan])
	break;
    if (ichan==channellist.size()) {
      cout << "...skipping."  << endl;
      continue;  // check if on the list of channels to be done
    }

    int massgev=isinterp ? interpolatedmasspts[imass] : masspts[imass];

    // insert into existing cards data map:

    map<int,CardData_t>::iterator it = m_cards.find(massgev);
    if (it == m_cards.end()) {                                    // new masspoint

      CardData_t card  = makeNewCard(h1,procname,systname,ichanref);

      m_cards[massgev] = card;

    } else {                                                 // existing masspoint

      addToCard(it->second,h1,procname,systname,ichanref,ichan,channellist.size());

    } // existing masspoint

    cout  << endl;
  } // key loop


  map<int,CardData_t>::iterator it;
  for (it=m_cards.begin();
       it!=m_cards.end();
       it++) {
    addSystematics(it->first, it->second,channellist);
  }

}                                                           // makeDataCardContent

//================================================================================

void
makeDataCardFiles(int argc, char*argv[])
{
  TString infname=TString(argv[1]);//"hww-histo-shapes-putconfiginfohere.root")
  TFile *fp = new TFile(infname);

  TString cfgtag;

#ifdef ISHWW
  //TString prefix("hww-histo-shapes-");
  TString prefix("hwwlvjj.input_");
#else
  TString prefix("mjj-histo-shapes-");
#endif //ISHWW
  if (infname.Contains(prefix)) {
    int len = prefix.Length();
    //cout << len << " " << infname.Last('/')+len+1 << " " << infname.Last('.')-infname.Last('/')-len-1 << endl;

    if (infname.Contains("/"))
      cfgtag = infname(infname.Last('/')+len+1,infname.Last('.')-infname.Last('/')-len-1);
    else
      cfgtag = infname(len,infname.Last('.')-len);
    cout << "cfgtag = " << cfgtag << endl;
  }
  
  if (fp->IsZombie()) {
    cerr << "Couldn't open file " << string(infname) << endl;
    exit(-1);
  }

  vector<int> channellist;
  for (int ichan=0; ichan<NUMCHAN; ichan++)
    if (argc > 2) {
      for( int j=2; j<argc; j++ )
	if (!strcmp(argv[j],channames[ichan])) {
	  channellist.push_back(ichan);
	  break;
	}
    } else
      channellist.push_back(ichan);
  
  if (!channellist.size()) {
    cerr << " no known channels found, buh-bye!" << endl;
    exit(-1);
  }

#ifdef ISHWW
  // needed for theoretical uncertainties
  readHxsTable   (Form("ggHtable%dtev.txt",beamcomenergytev));
  readHxsTable   (Form("vbfHtable%dtev.txt",beamcomenergytev)); // , scalefrom7to8tev);
  readJetBinErrTable("jetbinerrtable.txt");
#endif //ISHWW

  map<int,CardData_t> m_cards;
  makeDataCardContent(fp,m_cards,channellist);

  map<int,CardData_t>::iterator it;
  for (it=m_cards.begin();
       it!=m_cards.end();
       it++) {

#ifdef ISHWW
    it->second.shapespecs.push_back
      (ShapeFiles_t("*","*",infname,
		    "$PROCESS_$CHANNEL_Mass_$MASS", 
		    "$PROCESS_$CHANNEL_Mass_$MASS_$SYSTEMATIC")
       );
#else
    it->second->shapespecs.push_back
      (ShapeFiles_t("*","*",infname,
		    "$PROCESS_$CHANNEL",
		    "$PROCESS_$CHANNEL_$SYSTEMATIC")
       );
#endif //ISHWW

    fmtDataCardFile(it->first,it->second,cfgtag);
  }
  
}                                                             // makeDataCardFiles

#ifdef MAIN
//================================================================================

#define DEBUG 1

int main(int argc, char* argv[]) {
  if (argc<2) {
    printf("Usage: %s input.root\n",argv[0]);
    return 1;
  }
#ifdef DEBUG
  for (int i=0; i<argc; i++) printf("%s ", argv[i]);
  printf ("\n");
#endif

  //hwwshapes(argv[1],"hww-histo-shapes-TH1.root");

  makeDataCardFiles(argc, argv); // "hww-histo-shapes-TH1.root");
  return 0;
}
#endif //MAIN
