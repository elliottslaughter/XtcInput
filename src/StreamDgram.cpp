#include "MsgLogger/MsgLogger.h"
#include "XtcInput/StreamDgram.h"

namespace {
  char *logger = "StreamDgram";
  
};

namespace XtcInput {

StreamDgramCmp::StreamDgramCmp(const boost::shared_ptr<ExperimentClockDiffMap> expClockDiff, 
                                 unsigned maxClockDriftSeconds) 
  : m_expClockDiff(expClockDiff), m_fidCompare(maxClockDriftSeconds) {
  DgramCategory LD(L1Accept, DAQ);
  DgramCategory LC(L1Accept, controlUnderDAQ);
  DgramCategory LI(L1Accept, controlIndependent);
  DgramCategory TD(otherTrans, DAQ);
  DgramCategory TC(otherTrans, controlUnderDAQ);
  DgramCategory TI(otherTrans, controlIndependent);

  /* -------------------------------------------------------------------
     Below we encode the 21 cases for comparing the 6 Dgram catagorires (LD, LC, LI, TD, TC, TI) 
     defined above, against one another (there are 36 pairs from these 6 categories, but the
     compare method does not depend on the order of the pair: TD vs TI is the same as TI vs TD.

     The 21 cases cover all combinations we may see when merging dgrams from streams. 
     Issues that go into the merging rules:
     *  There may be multiple C streams (s80, s81)
     *  Not all L1 accepts in a C stream will have a matching L1 Accept in the DAQ stream. 
        We want to order them properly, the consumer class can decide if it wants to use 
        non-matching C stream L1 accepts.
     *  Comparing a C stream L1 accept against D or C stream Transitions requires history.
        The clocks are different and fiducials in both are not available, and the C stream
        L1 accept need not have a matching L1 in the Daq stream. This is when the block number 
        is used.
     * a T vs L comparison from D to I can't be done without help. They have different
       clocks and fiducials in both are not available. These comparisions will use the expClockDiff
       map, and throw an exception if a clockDiff is not available.
     ------------------------------------------------------------------- */
  m_LUT[makeDgramCategoryAB(LD,LD)] = clockCmp;
  m_LUT[makeDgramCategoryAB(LD,LC)] = fidCmp;
  m_LUT[makeDgramCategoryAB(LD,LI)] = fidCmp;
  m_LUT[makeDgramCategoryAB(LD,TD)] = clockCmp;
  m_LUT[makeDgramCategoryAB(LD,TC)] = clockCmp;
  m_LUT[makeDgramCategoryAB(LD,TI)] = mapCmp;

  m_LUT[makeDgramCategoryAB(LC,LC)] = fidCmp;
  m_LUT[makeDgramCategoryAB(LC,LI)] = fidCmp;
  m_LUT[makeDgramCategoryAB(LC,TD)] = blockCmp;
  m_LUT[makeDgramCategoryAB(LC,TC)] = blockCmp;
  m_LUT[makeDgramCategoryAB(LC,TI)] = mapCmp; 

  m_LUT[makeDgramCategoryAB(LI,LI)] = clockCmp;
  m_LUT[makeDgramCategoryAB(LI,TD)] = mapCmp;
  m_LUT[makeDgramCategoryAB(LI,TC)] = mapCmp;
  m_LUT[makeDgramCategoryAB(LI,TI)] = clockCmp;

  m_LUT[makeDgramCategoryAB(TD,TD)] = clockCmp;
  m_LUT[makeDgramCategoryAB(TD,TC)] = clockCmp;
  m_LUT[makeDgramCategoryAB(TD,TI)] = mapCmp;

  m_LUT[makeDgramCategoryAB(TC,TC)] = clockCmp;
  m_LUT[makeDgramCategoryAB(TC,TI)] = mapCmp;

  m_LUT[makeDgramCategoryAB(TI,TI)] = clockCmp;
}
  
StreamDgramCmp::DgramCategory StreamDgramCmp::getDgramCategory(const StreamDgram &dg) {
  if (dg.empty()) {
    MsgLog(logger, warning, "getDgramCategory called on empty dgram");
    return StreamDgramCmp::DgramCategory(L1Accept, DAQ);
  }

  TransitionType trans;
  if (dg.dg()->seq.service() == Pds::TransitionId::L1Accept) {
    trans = L1Accept;
  } else {
    trans = otherTrans;
  }
  return StreamDgramCmp::DgramCategory(trans, dg.streamType());
}

StreamDgramCmp::DgramCategoryAB StreamDgramCmp::makeDgramCategoryAB(DgramCategory a, DgramCategory b) {
  return StreamDgramCmp::DgramCategoryAB(a,b);
}

// implement greater than, 
bool StreamDgramCmp::operator()(const StreamDgram &a, const StreamDgram &b) const {
  // two empty datagrams are equal to one another
  if (a.empty() and b.empty()) return false;
 
  // an empty dgram is always greater than a non-empty one, empty dgrams should 
  // appear last in an ordered list of dgrams
  if (a.empty()) return true;
  if (b.empty()) return false;

  StreamDgramCmp::DgramCategory dgramCategA = getDgramCategory(a);
  StreamDgramCmp::DgramCategory dgramCategB = getDgramCategory(b);
  StreamDgramCmp::DgramCategoryAB dgramCategAB = makeDgramCategoryAB(dgramCategA, dgramCategB);
  std::map<DgramCategoryAB, CompareMethod>::const_iterator pos = m_LUT.find(dgramCategAB);
  if (pos == m_LUT.end()) {
    StreamDgramCmp::DgramCategoryAB dgramCategBA = makeDgramCategoryAB(dgramCategB, dgramCategA);
    pos = m_LUT.find(dgramCategBA);
    if (pos == m_LUT.end()) throw UnknownCmp(ERR_LOC);
  }
  StreamDgramCmp::CompareMethod compareMethod = pos->second;
  
  switch (compareMethod) {
  case clockCmp:
    return doClockCmp(a,b);
  case fidCmp:
    return doFidCmp(a,b);
  case blockCmp:
    return doBlockCmp(a,b);
  case mapCmp:
    return doMapCmp(a,b);
  }

  MsgLog(logger, fatal, "StreamDgramCmp: unexpected error. compare method in look up table = " 
         << int(compareMethod) << " was not handled in switch statement");
  return false;
}

// return true is a > b
bool StreamDgramCmp::doClockCmp(const StreamDgram &a, const StreamDgram &b) const
{ 
  if (a.empty() or b.empty()) throw psana::Exception(ERR_LOC, "StreamDgramCmp: empty dgs");
  const Pds::ClockTime & clockA = a.dg()->seq.clock();
  const Pds::ClockTime & clockB = b.dg()->seq.clock();
  return (clockA > clockB);
}

// return true is a > b
bool StreamDgramCmp::doFidCmp(const StreamDgram &a, const StreamDgram &b) const
{ 
  if (a.empty() or b.empty()) throw psana::Exception(ERR_LOC, "StreamDgramCmp: empty dgs");
  return (m_fidCompare.fiducialsGreater(*a.dg(), *b.dg()));
}

bool StreamDgramCmp::doBlockCmp(const StreamDgram &a, const StreamDgram &b) const
{ 
  if (a.empty() or b.empty()) throw psana::Exception(ERR_LOC, "StreamDgramCmp: empty dgs");

  TransitionType transA = getDgramCategory(a).first;
  TransitionType transB = getDgramCategory(b).first;

  if (((transA == L1Accept) and (transB == L1Accept)) or
      ((transA == otherTrans) and (transB == otherTrans))) {
    throw psana::Exception(ERR_LOC, "DoBlockCmp: both datagrams are "
                           "either L1Accept or otherTrans. They must be mixed");
  }

  // first compare runs. Block numbers can only be compared between datagrams in the same
  // xtcfile or run. Prior runs may not have recorded the s80 stream (or possibly the 
  // DAQ streams? though unlikely) so a running block number could get out of sync. The block 

  // compare runs
  unsigned runA = a.file().run();
  unsigned runB = b.file().run();

  double secondsA = a.dg()->seq.clock().asDouble();
  double secondsB = b.dg()->seq.clock().asDouble();
  double AminusB = secondsA - secondsB;

  if (runA < runB) {
    if (AminusB > m_fidCompare.maxClockDriftSeconds()) {
      MsgLog(logger, warning, "doBlockCmp: dgram A is in earler run but clock is more than "
             << m_fidCompare.maxClockDriftSeconds() << " seconds later than dgram B");
    }
    return false;
  }
  if (runA > runB) {
    if (AminusB < (-1.0*double(m_fidCompare.maxClockDriftSeconds()))) {
      MsgLog(logger, warning, "doBlockCmp: dgram A is in later run but clock is more than "
             << m_fidCompare.maxClockDriftSeconds() << " seconds earlier than dgram B");
    }
    return true;
  }

  // same run, compare block number.

  if ((transA == L1Accept) and (transB == otherTrans)) {
    return ( (a.L1Block()) >= (b.L1Block()));
  } 
  // (transA == otherTrans) and (transB == L1Accept)
  return ( (a.L1Block()) > (b.L1Block()));
}

bool StreamDgramCmp::doMapCmp(const StreamDgram &a, const StreamDgram &b) const
{ 
  if (a.empty() or b.empty()) throw psana::Exception(ERR_LOC, "StreamDgramCmp: empty dgs");
  if (not m_expClockDiff) throw psana::Exception(ERR_LOC, "doMapCmp: expClockDiff map is null");
  unsigned expA = a.file().expNum();
  unsigned expB = b.file().expNum();
  if ((expA == 0) or (expB == 0)) throw psana::Exception(ERR_LOC, "doMapCmp: an experiment number is 0");
  ExperimentPair experimentsAB(expA, expB);
  bool abInMap = true;
  ExperimentClockDiffMap::const_iterator pos = m_expClockDiff->find(experimentsAB);
  if (pos == m_expClockDiff->end()) {
    ExperimentPair experimentsBA(expB, expA);
    pos = m_expClockDiff->find(experimentsBA);
    if (pos == m_expClockDiff->end()) {
      throw NoClockDiff(ERR_LOC, expA, expB);
    }
    abInMap = false;
  }
  // now we need to add the clockDiff found in the map to a's clockTime (if abInMap is true)
  // or add it to the clockTime for b's clockTime (if abInMap is false) and do the 
  // normal clock comparison
  throw psana::Exception(ERR_LOC, "doMapCmp: not implmented");
}

bool StreamDgramCmp::sameEvent(const StreamDgram &a, const StreamDgram &b) const {
  if (a.empty() and b.empty()) {
    MsgLog(logger, warning, "sameEvent: comparing two empty dgrams");
    return true;
  }
  if  (a.empty() or b.empty()) {
    MsgLog(logger, warning, "sameEvent: comparing an empty dgram to a non-empty dgram");
    return false;
  }
  throw psana::Exception(ERR_LOC, "sameEvent: not implmented");
}
  
};