///////////////////////////////////////////////////////////////////
// SubtractedVolumeBounds.cxx, ACTS project
///////////////////////////////////////////////////////////////////

// Geometry module
#include "ACTS/Volumes/SubtractedVolumeBounds.h"
#include "ACTS/Volumes/CombinedVolumeBounds.h"
#include "ACTS/Volumes/VolumeExcluder.h"
#include "ACTS/Volumes/SimplePolygonBrepVolumeBounds.h"
#include "ACTS/Surfaces/Surface.h"
#include "ACTS/Surfaces/PlaneSurface.h"
#include "ACTS/Surfaces/CylinderSurface.h"
#include "ACTS/Surfaces/DiscSurface.h"
#include "ACTS/Surfaces/RadialBounds.h"
#include "ACTS/Surfaces/EllipseBounds.h"
#include "ACTS/Surfaces/SubtractedPlaneSurface.h"
#include "ACTS/Surfaces/SubtractedCylinderSurface.h"
#include "ACTS/Surfaces/RectangleBounds.h"
#include "ACTS/Volumes/Volume.h"
#include "ACTS/Volumes/CylinderVolumeBounds.h"
// STD/STL
#include <iostream>
#include <math.h>

Acts::SubtractedVolumeBounds::SubtractedVolumeBounds() :
 VolumeBounds(),
 m_outer(nullptr),
 m_inner(nullptr),
 m_boundsOrientation()
{}

Acts::SubtractedVolumeBounds::SubtractedVolumeBounds(Volume* vol1,Volume* vol2) :
 VolumeBounds(),
 m_outer(vol1),
 m_inner(vol2),
 m_boundsOrientation()
{}

Acts::SubtractedVolumeBounds::SubtractedVolumeBounds(const Acts::SubtractedVolumeBounds& bobo) :
 VolumeBounds(),
 m_outer(bobo.m_outer),
 m_inner(bobo.m_inner),
 m_boundsOrientation()
{
  m_boundsOrientation.resize(bobo.m_boundsOrientation.size());
  for (unsigned int i=0;i<bobo.m_boundsOrientation.size();i++) m_boundsOrientation[i]=bobo.m_boundsOrientation[i];
}

Acts::SubtractedVolumeBounds::~SubtractedVolumeBounds()
{
  m_boundsOrientation.clear();
  delete m_outer;
  delete m_inner;
}

Acts::SubtractedVolumeBounds& Acts::SubtractedVolumeBounds::operator=(const Acts::SubtractedVolumeBounds& bobo)
{
  if (this!=&bobo){
    m_outer             = bobo.m_outer;
    m_inner             = bobo.m_inner;
    m_boundsOrientation = bobo.m_boundsOrientation;
    m_boundsOrientation.resize(bobo.m_boundsOrientation.size());
    for (unsigned int i=0;i<bobo.m_boundsOrientation.size();i++) m_boundsOrientation[i]=bobo.m_boundsOrientation[i];
 }
  return *this;
}

const std::vector<const Acts::Surface*>* Acts::SubtractedVolumeBounds::decomposeToSurfaces(std::shared_ptr<Acts::Transform3D> transformPtr) const
{

    Acts::Transform3D transf = ( transformPtr == nullptr) ? Acts::Transform3D::Identity() : (*transformPtr.get());

    // get surfaces for outer boundaries
    std::shared_ptr<Acts::Transform3D> outerTransform(new Acts::Transform3D(transf*m_outer->transform()));
    const std::vector<const Acts::Surface*>* outerSurfaces = m_outer->volumeBounds().decomposeToSurfaces(outerTransform);
    // get surfaces for inner boundaries
    std::shared_ptr<Acts::Transform3D> innerTransform(new Acts::Transform3D(transf*m_inner->transform()));
    const std::vector<const Acts::Surface*>* innerSurfaces = m_inner->volumeBounds().decomposeToSurfaces(innerTransform);
    std::vector<unsigned int> subtrInner;
    std::vector<const Acts::Surface*>* retsf = new std::vector<const Acts::Surface*>;

    unsigned int nSurf = outerSurfaces->size() + innerSurfaces->size();
    m_boundsOrientation.resize(nSurf);

    const Acts::CylinderVolumeBounds*   cylVol = dynamic_cast<const Acts::CylinderVolumeBounds*> (&(m_outer->volumeBounds()));
    const Acts::SimplePolygonBrepVolumeBounds*   spbVol = dynamic_cast<const Acts::SimplePolygonBrepVolumeBounds*> (&(m_outer->volumeBounds()));
    const Acts::CombinedVolumeBounds*   comVol = dynamic_cast<const Acts::CombinedVolumeBounds*> (&(m_outer->volumeBounds()));
    const Acts::SubtractedVolumeBounds* subVol = dynamic_cast<const Acts::SubtractedVolumeBounds*> (&(m_outer->volumeBounds()));

    // loop over 'outer' boundary surfaces; modified by subtracted volume
    for (unsigned int out=0; out < outerSurfaces->size(); out++) {
        const SubtractedPlaneSurface* splo = dynamic_cast<const SubtractedPlaneSurface*> ((*outerSurfaces)[out]);
        const PlaneSurface* plo = dynamic_cast<const PlaneSurface*> ((*outerSurfaces)[out]);
        const SubtractedCylinderSurface* sclo = dynamic_cast<const SubtractedCylinderSurface*> ((*outerSurfaces)[out]);
        const CylinderSurface* clo = dynamic_cast<const CylinderSurface*> ((*outerSurfaces)[out]);
        const DiscSurface* dlo = dynamic_cast<const DiscSurface*> ((*outerSurfaces)[out]);
        if (!(splo || plo || sclo || clo || dlo )) {
            throw std::runtime_error("Unhandled surface.");
        }
        // resolve bounds orientation : copy from combined/subtracted, swap inner cyl, swap bottom spb
        if (comVol) m_boundsOrientation[out]=comVol->boundsOrientation()[out];
        else if (subVol) m_boundsOrientation[out]=subVol->boundsOrientation()[out];
        else if (cylVol && clo && out==3 ) m_boundsOrientation[out]= false;
        else if (spbVol && out==0 ) m_boundsOrientation[out]= false;
        else m_boundsOrientation[out] = true;
        //
        Acts::Volume* innerSub = createSubtractedVolume((*outerSurfaces)[out]->transform().inverse()*transf,m_inner) ;

        if ( splo || sclo ) { // multiple subtraction
            std::shared_ptr<Acts::AreaExcluder> vEx;
            bool shared = false;
            if (splo) {
                vEx = splo->subtractedVolume();
                shared   = splo->shared();
            }
            if (sclo) {
                vEx = sclo->subtractedVolume();
                shared   = sclo->shared();
            }
            //vEx.addRef();
            const Acts::VolumeExcluder* volExcl = dynamic_cast<const Acts::VolumeExcluder*> (vEx.get());
            if (!volExcl) throw std::logic_error("Not a VolumeExcluder");
            Acts::Volume* outerSub = new Acts::Volume(*volExcl->volume());

            Acts::Volume* comb_sub = 0;
            if (!shared) comb_sub = new Acts::Volume(0,new Acts::CombinedVolumeBounds(innerSub,outerSub,false));
            else         comb_sub = new Acts::Volume(0,new Acts::SubtractedVolumeBounds(outerSub,innerSub));
            Acts::VolumeExcluder* volEx = new Acts::VolumeExcluder(comb_sub);
            if (splo) retsf->push_back(new Acts::SubtractedPlaneSurface(*splo,volEx,shared));
            if (sclo) retsf->push_back(new Acts::SubtractedCylinderSurface(*sclo,volEx,shared));
        } else {
            Acts::VolumeExcluder* volEx = new Acts::VolumeExcluder(innerSub);
            if (plo) retsf->push_back(new Acts::SubtractedPlaneSurface(*plo,volEx,false));
            if (clo) retsf->push_back(new Acts::SubtractedCylinderSurface(*clo,volEx,false));
            if (dlo) {
                // turn disc into ellipse for simplification
                const RadialBounds* db = dynamic_cast<const RadialBounds*> (&(dlo->bounds()));
                if (!db) throw std::logic_error("Not RadialBounds");
                EllipseBounds* eb = new EllipseBounds(db->rMin(),db->rMin(),db->rMax(),db->rMax(),db->halfPhiSector());
                plo = new PlaneSurface(std::shared_ptr<Acts::Transform3D>(new Acts::Transform3D(dlo->transform())),eb);
                retsf->push_back(new Acts::SubtractedPlaneSurface(*plo,volEx,false));
                delete plo;
            }
        }
    }

    // loop over 'inner' boundary surfaces; include only if represent a new surface
    // change: include allways otherwise orientation messed up
    // bonus : solves 'double boundary' problem

    cylVol = dynamic_cast<const Acts::CylinderVolumeBounds*> (&(m_inner->volumeBounds()));
    spbVol = dynamic_cast<const Acts::SimplePolygonBrepVolumeBounds*> (&(m_inner->volumeBounds()));
    comVol = dynamic_cast<const Acts::CombinedVolumeBounds*> (&(m_inner->volumeBounds()));
    subVol = dynamic_cast<const Acts::SubtractedVolumeBounds*> (&(m_inner->volumeBounds()));
    unsigned int nOut = outerSurfaces->size();

    for (unsigned int in=0; in< innerSurfaces->size(); in++) {
        const SubtractedPlaneSurface* spli = dynamic_cast<const SubtractedPlaneSurface*> ((*innerSurfaces)[in]);
        const PlaneSurface* pli = dynamic_cast<const PlaneSurface*> ((*innerSurfaces)[in]);
        const SubtractedCylinderSurface* scli = dynamic_cast<const SubtractedCylinderSurface*> ((*innerSurfaces)[in]);
        const CylinderSurface* cli = dynamic_cast<const CylinderSurface*> ((*innerSurfaces)[in]);
        const DiscSurface* dli = dynamic_cast<const DiscSurface*> ((*innerSurfaces)[in]);
        // resolve bounds orientation : copy from combined/subtracted, swap inner cyl, swap bottom spb, swap all
        if (comVol) m_boundsOrientation[nOut+in]=!comVol->boundsOrientation()[in];
        else if (subVol) m_boundsOrientation[nOut+in]=!subVol->boundsOrientation()[in];
        else if (cylVol && cli && in==3 ) m_boundsOrientation[nOut+in]= true;
        else if (spbVol && in==0 ) m_boundsOrientation[nOut+in]= true;
        else m_boundsOrientation[nOut+in]= false;
        //
        Acts::Volume* outerSub = createSubtractedVolume((*innerSurfaces)[in]->transform().inverse()*transf, m_outer);

        if ( spli || scli ) {
            bool shared = false;
            std::shared_ptr<Acts::AreaExcluder> vEx;
            if (spli) {
                vEx = spli->subtractedVolume();
                shared   = spli->shared();
            }
            if (scli) {
                vEx = scli->subtractedVolume();
                shared   = scli->shared();
            }
            //vEx.addRef();
            const Acts::VolumeExcluder* volExcl = dynamic_cast<const Acts::VolumeExcluder*> (vEx.get());
            if (!volExcl) throw std::logic_error("Not a VolumeExcluder");
            Acts::Volume* innerSub = new Acts::Volume(*volExcl->volume());

            // combined volume
            Acts::Volume* comb_sub=0;
            if (!shared) comb_sub = new Acts::Volume(0,new Acts::SubtractedVolumeBounds(outerSub,innerSub));
            else         comb_sub = new Acts::Volume(0,new Acts::CombinedVolumeBounds(innerSub,outerSub,true));
            Acts::VolumeExcluder* volEx = new Acts::VolumeExcluder(comb_sub);
            if (spli) retsf->push_back(new Acts::SubtractedPlaneSurface(*spli,volEx,true));
            if (scli) retsf->push_back(new Acts::SubtractedCylinderSurface(*scli,volEx,true));

        }
        else if (pli || cli){
            Acts::VolumeExcluder* volEx = new Acts::VolumeExcluder(outerSub);
            if (pli) retsf->push_back(new Acts::SubtractedPlaneSurface(*pli,volEx,true));
            if (cli) retsf->push_back(new Acts::SubtractedCylinderSurface(*cli,volEx,true));
        }
        else if (dli)  {
            // turn disc into ellipse for simplification
            const RadialBounds* db = dynamic_cast<const RadialBounds*> (&(dli->bounds()));
            if (!db) throw std::logic_error("Not RadialBounds");
            EllipseBounds* eb = new EllipseBounds(db->rMin(),db->rMin(),db->rMax(),db->rMax(),db->halfPhiSector());
            PlaneSurface pla(std::shared_ptr<Acts::Transform3D>(new Acts::Transform3D(dli->transform())),eb);
            Acts::VolumeExcluder* volEx = new Acts::VolumeExcluder(outerSub);
            retsf->push_back(new Acts::SubtractedPlaneSurface(pla,volEx,true));
        } else {
            throw std::runtime_error("Unhandled surface in Acts::SubtractedVolumeBounds::decomposeToSurfaces." );
        }
    }

    for (size_t i=0; i < outerSurfaces->size(); i++)
        delete (*outerSurfaces)[i];
    for (size_t i=0; i < innerSurfaces->size(); i++)
        delete (*innerSurfaces)[i];
    delete outerSurfaces;
    delete innerSurfaces;

    return retsf;
}

// ostream operator overload
std::ostream& Acts::SubtractedVolumeBounds::dump( std::ostream& sl ) const
{
    std::stringstream temp_sl;
    temp_sl << std::setiosflags(std::ios::fixed);
    temp_sl << std::setprecision(7);
    temp_sl << "Acts::SubtractedVolumeBounds: outer,inner ";
    sl << temp_sl.str();
    m_outer->volumeBounds().dump(sl);
    m_inner->volumeBounds().dump(sl);
    return sl;
}

Acts::Volume* Acts::SubtractedVolumeBounds::createSubtractedVolume(const Acts::Transform3D& transf, Acts::Volume* subtrVol) const
{
  Acts::Volume* subVol = 0;
  if (!subtrVol) return subVol;

  subVol = new Acts::Volume( *subtrVol, transf );

  return subVol;
}

