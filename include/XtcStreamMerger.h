#ifndef XTCINPUT_XTCSTREAMMERGER_H
#define XTCINPUT_XTCSTREAMMERGER_H

//--------------------------------------------------------------------------
// File and Version Information:
// 	$Id$
//
// Description:
//	Class XtcStreamMerger.
//
//------------------------------------------------------------------------

//-----------------
// C/C++ Headers --
//-----------------
#include <list>
#include <vector>
#include <iosfwd>
#include <boost/utility.hpp>

//----------------------
// Base Class Headers --
//----------------------

//-------------------------------
// Collaborating Class Headers --
//-------------------------------
#include "XtcInput/Dgram.h"
#include "XtcInput/XtcDechunk.h"
#include "XtcInput/XtcFileName.h"

//------------------------------------
// Collaborating Class Declarations --
//------------------------------------

//		---------------------
// 		-- Class Interface --
//		---------------------

namespace XtcInput {

/**
 *  Class responsible for merging of the datagrams from several
 *  XTC streams.
 *
 *  This software was developed for the LUSI project.  If you use all or
 *  part of it, please give an appropriate acknowledgment.
 *
 *  @see AdditionalClass
 *
 *  @version $Id$
 *
 *  @author Andrei Salnikov
 */

class XtcStreamMerger : boost::noncopyable {
public:

  /// Several merge modes supported:
  ///   OneStream - all files come from one stream, chunked
  ///   FilePerStream - single file per stream, no chunking
  ///   FileName - streams and chunks are determined from file names
  enum MergeMode { OneStream, NoChunking, FileName } ;
  
  /**
   *  @brief Make merge mode from string
   *  
   *  @throw InvalidMergeMode Thrown if string does not match the names 
   *    of enum constants
   */
  static MergeMode mergeMode(const std::string& str);

  // Default constructor
  XtcStreamMerger ( const std::list<XtcFileName>& files,
                 size_t maxDgSize,
                 MergeMode mode,
                 bool skipDamaged,
                 double l1OffsetSec = 0 ) ;

  // Destructor
  ~XtcStreamMerger () ;

  // read next datagram, return zero pointer after last file has been read,
  // throws exception for errors.
  Dgram::ptr next() ;

protected:

  // update time in datagram
  void updateDgramTime(Pds::Dgram& dgram) const ;

private:

  // Data members
  std::vector<XtcDechunk*> m_streams ;
  std::vector<Dgram::ptr> m_dgrams ;
  MergeMode m_mode ;
  int32_t m_l1OffsetSec ;
  int32_t m_l1OffsetNsec ;

};

/// Insertion operator for enum values
std::ostream&
operator<<(std::ostream& out, XtcStreamMerger::MergeMode mode);

} // namespace XtcInput

#endif // XTCINPUT_XTCSTREAMMERGER_H
