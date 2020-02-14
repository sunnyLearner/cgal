// Copyright (c) 2015 GeometryFactory (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
//
// $URL$
// $Id$
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-Commercial
//
//
// Author(s)     : Jane Tournois

#ifndef CGAL_POLYGON_MESH_PROCESSING_REMESH_H
#define CGAL_POLYGON_MESH_PROCESSING_REMESH_H

#include <CGAL/license/Polygon_mesh_processing/meshing_hole_filling.h>

#include <CGAL/disable_warnings.h>

#include <CGAL/Polygon_mesh_processing/internal/Isotropic_remeshing/remesh_impl.h>

#include <CGAL/Polygon_mesh_processing/internal/named_function_params.h>
#include <CGAL/Polygon_mesh_processing/internal/named_params_helper.h>

#ifdef CGAL_PMP_REMESHING_VERBOSE
#include <CGAL/Timer.h>
#endif

namespace CGAL {

namespace Polygon_mesh_processing {

/*!
* \ingroup PMP_meshing_grp
* @brief remeshes a triangulated region of a polygon mesh.
* This operation sequentially performs edge splits, edge collapses,
* edge flips, tangential relaxation and projection to the initial surface
* to generate a smooth mesh with a prescribed edge length.
*
* @tparam PolygonMesh model of `MutableFaceGraph`.
*         The descriptor types `boost::graph_traits<PolygonMesh>::%face_descriptor`
*         and `boost::graph_traits<PolygonMesh>::%halfedge_descriptor` must be
*         models of `Hashable`.
*         If `PolygonMesh`
  *  has an internal not writable property map
  *  for `CGAL::face_index_t` and no `face_index_map` is given
  *  as a named parameter, then the internal one must be initialized; else, it will be.
  *
* @tparam FaceRange range of `boost::graph_traits<PolygonMesh>::%face_descriptor`,
          model of `Range`. Its iterator type is `ForwardIterator`.
* @tparam NamedParameters a sequence of \ref pmp_namedparameters "Named Parameters"
*
* @param pmesh a polygon mesh with triangulated surface patches to be remeshed
* @param faces the range of triangular faces defining one or several surface patches to be remeshed
* @param target_edge_length the edge length that is targeted in the remeshed patch.
*        If `0` is passed then only the edge-flip, tangential relaxation, and projection steps will be done.
* @param np optional sequence of \ref pmp_namedparameters "Named Parameters" among the ones listed below
*
* @pre if constraints protection is activated, the constrained edges must
* not be longer than 4/3*`target_edge_length`
*
* \cgalNamedParamsBegin
*  \cgalParamBegin{geom_traits} a geometric traits class instance, model of `Kernel`.
*    Exact constructions kernels are not supported by this function.
*  \cgalParamEnd
*  \cgalParamBegin{vertex_point_map} the property map with the points associated
*    to the vertices of `pmesh`. Instance of a class model of `ReadWritePropertyMap`.
*  \cgalParamEnd
*  \cgalParamBegin{face_index_map} a property map containing the index of each face of `pmesh`
*  \cgalParamEnd
*  \cgalParamBegin{number_of_iterations} the number of iterations for the
*    sequence of atomic operations performed (listed in the above description)
*  \cgalParamEnd
*  \cgalParamBegin{edge_is_constrained_map} a property map containing the
*    constrained-or-not status of each edge of `pmesh`. A constrained edge can be split
*    or collapsed, but not flipped, nor its endpoints moved by smoothing.
*    Sub-edges generated by splitting are set to be constrained.
*    Note that patch boundary edges (i.e. incident to only one face in the range)
*    are always considered as constrained edges.
*  \cgalParamEnd
*  \cgalParamBegin{vertex_is_constrained_map} a property map containing the
*    constrained-or-not status of each vertex of `pmesh`. A constrained vertex
*    cannot be modified at all during remeshing
*  \cgalParamEnd
*  \cgalParamBegin{protect_constraints} If `true`, the edges set as constrained
*     in `edge_is_constrained_map` (or by default the boundary edges)
*     are not split nor collapsed during remeshing.
*     Note that around constrained edges that have their length higher than
*     twice `target_edge_length`, remeshing will fail to provide
*     good quality results. It can even fail to terminate because of cascading vertex
*     insertions.
*  \cgalParamEnd
*  \cgalParamBegin{collapse_constraints} If `true`, the edges set as constrained
*     in `edge_is_constrained_map` (or by default the boundary edges)
*     are collapsed during remeshing. This value is ignored if `protect_constraints` is true;
*  \cgalParamEnd
*  \cgalParamBegin{face_patch_map} a property map with the patch id's associated to the
     faces of `faces`. Instance of a class model of `ReadWritePropertyMap`. It gets
     updated during the remeshing process while new faces are created.
*  \cgalParamEnd
*  \cgalParamBegin{number_of_relaxation_steps} the number of iterations of tangential
*    relaxation that are performed at each iteration of the remeshing process
*  \cgalParamEnd
*  \cgalParamBegin{relax_constraints} If `true`, the end vertices of the edges set as
*    constrained in `edge_is_constrained_map` and boundary edges move along the
*    constrained polylines they belong to.
*  \cgalParamEnd
*  \cgalParamBegin{do_project} a boolean that sets whether vertices should be reprojected
*    on the input surface after creation or displacement.
*  \cgalParamEnd
*  \cgalParamBegin{projection_functor}
*  A function object used to project input vertices (moved by the smoothing) and created vertices.
*  It must have `%Point_3 operator()(vertex_descriptor)`, `%Point_3` being the value type
*  of the vertex point map.
*  If not provided, vertices are projected on the input surface mesh.
*  \cgalParamEnd
* \cgalNamedParamsEnd
*
* @sa `split_long_edges()`
*
*@todo Deal with exact constructions Kernel. The only thing that makes sense is to
*      guarantee that the output vertices are exactly on the input surface.
*      To do so, we can do every construction in `double`, and use an exact process for
*      projection. For each vertex, the `AABB_tree` would be used in an inexact manner
*      to find the triangle on which projection has to be done. Then, use
*      `CGAL::intersection(triangle, line)` in the exact constructions kernel to
*      get a point which is exactly on the surface.
*
*/
template<typename PolygonMesh
       , typename FaceRange
       , typename NamedParameters>
void isotropic_remeshing(const FaceRange& faces
                       , const double& target_edge_length
                       , PolygonMesh& pmesh
                       , const NamedParameters& np)
{
  if (boost::begin(faces)==boost::end(faces))
    return;

  typedef PolygonMesh PM;
  typedef typename boost::graph_traits<PM>::vertex_descriptor vertex_descriptor;
  typedef typename boost::graph_traits<PM>::edge_descriptor edge_descriptor;
  using parameters::get_parameter;
  using parameters::choose_parameter;

#ifdef CGAL_PMP_REMESHING_VERBOSE
  std::cout << std::endl;
  CGAL::Timer t;
  std::cout << "Remeshing parameters...";
  std::cout.flush();
  t.start();
#endif

  static const bool need_aabb_tree =
    parameters::is_default_parameter(get_parameter(np, internal_np::projection_functor));

  typedef typename GetGeomTraits<PM, NamedParameters>::type GT;
  GT gt = choose_parameter(get_parameter(np, internal_np::geom_traits), GT());

  typedef typename GetVertexPointMap<PM, NamedParameters>::type VPMap;
  VPMap vpmap = choose_parameter(get_parameter(np, internal_np::vertex_point),
                             get_property_map(vertex_point, pmesh));
typedef typename Default_face_index_map<NamedParameters, PolygonMesh>::type FIMap;
  FIMap fimap =
      CGAL::Polygon_mesh_processing::get_initialized_face_index_map(pmesh, np);

  typedef typename internal_np::Lookup_named_param_def <
      internal_np::edge_is_constrained_t,
      NamedParameters,
      Constant_property_map<edge_descriptor, bool> // default (no constraint pmap)
    > ::type ECMap;
  ECMap ecmap = choose_parameter(get_parameter(np, internal_np::edge_is_constrained),
                                 Constant_property_map<edge_descriptor, bool>(false));

  typedef typename internal_np::Lookup_named_param_def <
      internal_np::vertex_is_constrained_t,
      NamedParameters,
      Constant_property_map<vertex_descriptor, bool> // default (no constraint pmap)
    > ::type VCMap;
  VCMap vcmap = choose_parameter(get_parameter(np, internal_np::vertex_is_constrained),
                                 Constant_property_map<vertex_descriptor, bool>(false));

  bool protect = choose_parameter(get_parameter(np, internal_np::protect_constraints), false);
  typedef typename internal_np::Lookup_named_param_def <
      internal_np::face_patch_t,
      NamedParameters,
      internal::Connected_components_pmap<PM, FIMap>//default
    > ::type FPMap;
  FPMap fpmap = choose_parameter(
    get_parameter(np, internal_np::face_patch),
    internal::Connected_components_pmap<PM, FIMap>(faces, pmesh, ecmap, fimap,
      parameters::is_default_parameter(get_parameter(np, internal_np::face_patch)) && (need_aabb_tree
#if !defined(CGAL_NO_PRECONDITIONS)
      || protect // face patch map is used to identify patch border edges to check protected edges are short enough
#endif
    ) ) );

  double low = 4. / 5. * target_edge_length;
  double high = 4. / 3. * target_edge_length;

#if !defined(CGAL_NO_PRECONDITIONS)
  if(protect)
  {
    std::string msg("Isotropic remeshing : protect_constraints cannot be set to");
    msg.append(" true with constraints larger than 4/3 * target_edge_length.");
    msg.append(" Remeshing aborted.");
    CGAL_precondition_msg(
      internal::constraints_are_short_enough(pmesh, ecmap, vpmap, fpmap, high),
      msg.c_str());
  }
#endif

#ifdef CGAL_PMP_REMESHING_VERBOSE
  t.stop();
  std::cout << "\rRemeshing parameters done ("<< t.time() <<" sec)" << std::endl;
  std::cout << "Remesher construction...";
  std::cout.flush();
  t.reset(); t.start();
#endif

  typename internal::Incremental_remesher<PM, VPMap, GT, ECMap, VCMap, FPMap, FIMap>
    remesher(pmesh, vpmap, gt, protect, ecmap, vcmap, fpmap, fimap, need_aabb_tree);
  remesher.init_remeshing(faces);

#ifdef CGAL_PMP_REMESHING_VERBOSE
  t.stop();
  std::cout << " done ("<< t.time() <<" sec)." << std::endl;
#endif

  bool collapse_constraints = choose_parameter(get_parameter(np, internal_np::collapse_constraints), true);
  unsigned int nb_iterations = choose_parameter(get_parameter(np, internal_np::number_of_iterations), 1);
  bool smoothing_1d = choose_parameter(get_parameter(np, internal_np::relax_constraints), false);
  unsigned int nb_laplacian = choose_parameter(get_parameter(np, internal_np::number_of_relaxation_steps), 1);

#ifdef CGAL_PMP_REMESHING_VERBOSE
  std::cout << std::endl;
  std::cout << "Remeshing (size = " << target_edge_length;
  std::cout << ", #iter = " << nb_iterations << ")..." << std::endl;
  t.reset(); t.start();
#endif

  for (unsigned int i = 0; i < nb_iterations; ++i)
  {
#ifdef CGAL_PMP_REMESHING_VERBOSE
    std::cout << " * Iteration " << (i + 1) << " *" << std::endl;
#endif
    if (target_edge_length>0)
    {
      remesher.split_long_edges(high);
      remesher.collapse_short_edges(low, high, collapse_constraints);
    }
    remesher.equalize_valences();
    remesher.tangential_relaxation(smoothing_1d, nb_laplacian);
    if ( choose_parameter(get_parameter(np, internal_np::do_project), true) )
      remesher.project_to_surface(get_parameter(np, internal_np::projection_functor));
#ifdef CGAL_PMP_REMESHING_VERBOSE
    std::cout << std::endl;
#endif
  }

#ifdef CGAL_PMP_REMESHING_VERBOSE
  t.stop();
  std::cout << "Remeshing done (size = " << target_edge_length;
  std::cout << ", #iter = " << nb_iterations;
  std::cout << ", " << t.time() << " sec )." << std::endl;
#endif
}

template<typename PolygonMesh
       , typename FaceRange>
void isotropic_remeshing(
    const FaceRange& faces
  , const double& target_edge_length
  , PolygonMesh& pmesh)
{
  isotropic_remeshing(
    faces,
    target_edge_length,
    pmesh,
    parameters::all_default());
}

/*!
* \ingroup PMP_meshing_grp
* @brief splits the edges listed in `edges` into sub-edges
* that are not longer than the given threshold `max_length`.
*
* Note this function is useful to split constrained edges before
* calling `isotropic_remeshing()` with protection of constraints
* activated (to match the constrained edge length required by the
* remeshing algorithm to be guaranteed to terminate)
*
* @tparam PolygonMesh model of `MutableFaceGraph` that
*         has an internal property map for `CGAL::vertex_point_t`.
* @tparam EdgeRange range of `boost::graph_traits<PolygonMesh>::%edge_descriptor`,
*   model of `Range`. Its iterator type is `InputIterator`.
* @tparam NamedParameters a sequence of \ref pmp_namedparameters "Named Parameters"
*
* @param pmesh a polygon mesh
* @param edges the range of edges to be split if they are longer than given threshold
* @param max_length the edge length above which an edge from `edges` is split
*        into to sub-edges
* @param np optional \ref pmp_namedparameters "Named Parameters", amongst those described below

* \cgalNamedParamsBegin
*  \cgalParamBegin{vertex_point_map} the property map with the points associated
*    to the vertices of `pmesh`. Instance of a class model of `ReadWritePropertyMap`.
*  \cgalParamEnd
*  \cgalParamBegin{face_index_map} a property map containing the index of each face of `pmesh`
*  \cgalParamEnd
*  \cgalParamBegin{edge_is_constrained_map} a property map containing the
*    constrained-or-not status of each edge of `pmesh`. A constrained edge can be split,
*    and the sub-edges are set to be constrained.
*  \cgalParamEnd
* \cgalNamedParamsEnd
*
* @sa `isotropic_remeshing()`
*
*/
template<typename PolygonMesh
       , typename EdgeRange
       , typename NamedParameters>
void split_long_edges(const EdgeRange& edges
                    , const double& max_length
                    , PolygonMesh& pmesh
                    , const NamedParameters& np)
{
  typedef PolygonMesh PM;
  typedef typename boost::graph_traits<PM>::edge_descriptor edge_descriptor;
  typedef typename boost::graph_traits<PM>::vertex_descriptor vertex_descriptor;
  using parameters::choose_parameter;
  using parameters::get_parameter;

  typedef typename GetGeomTraits<PM, NamedParameters>::type GT;
  GT gt = choose_parameter(get_parameter(np, internal_np::geom_traits), GT());

  typedef typename GetVertexPointMap<PM, NamedParameters>::type VPMap;
  VPMap vpmap = choose_parameter(get_parameter(np, internal_np::vertex_point),
                             get_property_map(vertex_point, pmesh));

  typedef typename Default_face_index_map<NamedParameters, PolygonMesh>::type FIMap;
  FIMap fimap =
    CGAL::Polygon_mesh_processing::get_initialized_face_index_map(pmesh, np);

  typedef typename internal_np::Lookup_named_param_def <
        internal_np::edge_is_constrained_t,
        NamedParameters,
        Constant_property_map<edge_descriptor, bool> // default (no constraint pmap)
      > ::type ECMap;
  ECMap ecmap = choose_parameter(get_parameter(np, internal_np::edge_is_constrained),
                                 Constant_property_map<edge_descriptor, bool>(false));
  
  typename internal::Incremental_remesher<PM, VPMap, GT, ECMap,
    Constant_property_map<vertex_descriptor, bool>, // no constraint pmap
    internal::Connected_components_pmap<PM, FIMap>,
    FIMap
  >
    remesher(pmesh, vpmap, gt, false/*protect constraints*/, ecmap,
             Constant_property_map<vertex_descriptor, bool>(false),
             internal::Connected_components_pmap<PM, FIMap>(faces(pmesh), pmesh, ecmap, fimap, false),
             fimap,
             false/*need aabb_tree*/);

  remesher.split_long_edges(edges, max_length);
}

template<typename PolygonMesh, typename EdgeRange>
void split_long_edges(const EdgeRange& edges
                    , const double& max_length
                    , PolygonMesh& pmesh)
{
  split_long_edges(edges,
    max_length,
    pmesh,
    parameters::all_default());
}

} //end namespace Polygon_mesh_processing
} //end namespace CGAL

#include <CGAL/enable_warnings.h>

#endif
