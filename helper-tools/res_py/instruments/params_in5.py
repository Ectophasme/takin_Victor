#
# tests the resolution calculation
#
# @author Mecoli Victor <mecoli.ill.fr>
# @date feb-2025
# @license GPLv2
#
# ----------------------------------------------------------------------------
# Takin (inelastic neutron scattering software package)
# Copyright (C) 2017-2025  Tobias WEBER (Institut Laue-Langevin (ILL),
#                          Grenoble, France).
# Copyright (C) 2013-2017  Tobias WEBER (Technische Universitaet Muenchen
#                          (TUM), Garching, Germany).
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
# ----------------------------------------------------------------------------
#

import libs.helpers as helpers
import libs.tas as tas

import numpy as np


# Important parameters of IN5
# Hypothesis : perfect collimation of the incomming beem, point sample
IN5 = {
    "det_shape":"VCYL",
    "L_chopP12":[149.8e7, 0.],            # [distance, delta]
    "L_chopP2M1":[7800.6e7, 0.],           # [distance, delta]
    "L_chopM12":[54.8e7, 0.],              # [distance, delta]
    "L_chopM2S":[1229.5e7, 0.],            # [distance, delta]
    "rad_det":[4000.e7, 26e7], #[4000.e7, 0],              # [distance, delta]
    "theta_i":[0, 0], # [0., 2.04e-3],                 # [angle, delta]
    "phi_i":[0, 0], # [0., 1.25e-2],                   # [angle, delta]
    "delta_theta_f":6.5e-3, #0,
    "delta_z":30e7, #0,
    "prop_chopP":[np.deg2rad(9.), np.divide(7000.*np.pi, 30), np.divide(17000.*np.pi, 30)],    # [window angle, min rot speed, max rot speed]
    "prop_chopM":[np.deg2rad(3.25), np.divide(7000.*np.pi, 30), np.divide(17000.*np.pi, 30)],    # [window angle, min rot speed, max rot speed]
    "delta_time_det":0
}


# settings
ki = 2*np.pi/5  # 2.5
kf = 2*np.pi/5
Q = 1

z = 0
theta_f = tas.get_scattering_angle(ki, kf, Q) # ki, kf and Q because z = 0 so rotation in the xy plane
rot_speedP = np.divide(8500.*np.pi, 30)
rot_speedM = np.divide(8500.*np.pi, 30)
shape = IN5["det_shape"]

reso_method = "vio"    # "vio", "eck", "pop", or "cn"
verbose = True


#
# resolution parameters
# NOTE: not all parameters are used by all calculation backends
#
params = {
    # options
    "verbose" : verbose,

    # scattering triangle
    "ki" : ki,
    "kf" : kf,
    "E" : tas.get_E(ki, kf),
    "Q" : Q,

    # geometrical informations of TOF instruments
    "angles":IN5["theta_i"] + IN5["phi_i"] + [theta_f, IN5["delta_theta_f"]],
    "dist_PM":IN5["L_chopP12"] + IN5["L_chopP2M1"] + IN5["L_chopM12"],
    "dist_MS":IN5["L_chopM2S"],
    "dist_SD":IN5["rad_det"] + [z, IN5["delta_z"]],

    # chopper informations of TOF instruments
    "chopperP":IN5["prop_chopP"] + [rot_speedP],
    "chopperM":IN5["prop_chopM"] + [rot_speedM],

    "delta_time_det":IN5["delta_time_det"],

    # d spacings
    "mono_xtal_d" : 3.355,
    "ana_xtal_d" : 3.355,

     # scattering senses
    "mono_sense" : -1.,
    "sample_sense" : 1.,
    "ana_sense" : -1.,
    "mirror_Qperp" : False,

    # distances
    "dist_vsrc_mono" : 10. * helpers.cm2A,
    "dist_hsrc_mono" : 10. * helpers.cm2A,
    "dist_mono_sample" : 200. * helpers.cm2A,
    "dist_sample_ana" : 115. * helpers.cm2A,
    "dist_ana_det" : 85. * helpers.cm2A,

    # shapes
    "src_shape" : "rectangular",     # "rectangular" or "circular"
    "sample_shape" : "cylindrical",  # "cuboid" or "cylindrical"
    "det_shape" : shape, # "rectangular" or "circular" ; for violini : SPHERE, VCYL, HCYL

    # component sizes
    "src_w" : 6. * helpers.cm2A,
    "src_h" : 12. * helpers.cm2A,
    "mono_d" : 0.15 * helpers.cm2A,
    "mono_w" : 12. * helpers.cm2A,
    "mono_h" : 8. * helpers.cm2A,
    "sample_d" : 1. * helpers.cm2A,
    "sample_w" : 1. * helpers.cm2A,
    "sample_h" : 1. * helpers.cm2A,
    "ana_d" : 0.3 * helpers.cm2A,
    "ana_w" : 12. * helpers.cm2A,
    "ana_h" : 8. * helpers.cm2A,
    "det_w" : 1.5 * helpers.cm2A,
    "det_h" : 5. * helpers.cm2A,

    # horizontal collimation
    "coll_h_pre_mono" : 30. * helpers.min2rad,
    "coll_h_pre_sample" : 30. * helpers.min2rad,
    "coll_h_post_sample" : 30. * helpers.min2rad,
    "coll_h_post_ana" : 30. * helpers.min2rad,

    # vertical collimation
    "coll_v_pre_mono" : 30. * helpers.min2rad,
    "coll_v_pre_sample" : 30. * helpers.min2rad,
    "coll_v_post_sample" : 30. * helpers.min2rad,
    "coll_v_post_ana" : 30. * helpers.min2rad,

    # horizontal focusing
    "mono_curv_h" : 0.,
    "ana_curv_h" : 0.,
    "mono_is_curved_h" : False,
    "ana_is_curved_h" : False,
    "mono_is_optimally_curved_h" : False,
    "ana_is_optimally_curved_h" : False,
    "mono_curv_h_formula" : None,
    "ana_curv_h_formula" : None,


    # vertical focusing
    "mono_curv_v" : 0.,
    "ana_curv_v" : 0.,
    "mono_is_curved_v" : True,
    "ana_is_curved_v" : False,
    "mono_is_optimally_curved_v" : True,
    "ana_is_optimally_curved_v" : False,
    "mono_curv_v_formula" : None,
    "ana_curv_v_formula" : None,

    # guide before monochromator
    "use_guide" : True,
    "guide_div_h" : 15. *helpers.min2rad,
    "guide_div_v" : 15. *helpers.min2rad,

    # horizontal mosaics
    "mono_mosaic" : 45. *helpers.min2rad,
    "sample_mosaic" : 30. *helpers.min2rad,
    "ana_mosaic" : 45. *helpers.min2rad,

    # vertical mosaics
    "mono_mosaic_v" : 45. *helpers.min2rad,
    "sample_mosaic_v" : 30. *helpers.min2rad,
    "ana_mosaic_v" : 45. *helpers.min2rad,

    # calculate R0 factor (not needed if only the ellipses are to be plotted)
    "calc_R0" : True,

    # crystal reflectivities
    # TODO, so far always 1
    "dmono_refl" : 1.,
    "dana_effic" : 1.,

    # off-center scattering
    # WARNING: while this is calculated, it is not yet considered in the ellipse plots
    "pos_x" : 0. * helpers.cm2A,
    "pos_y" : 0. * helpers.cm2A,
    "pos_z" : 0. * helpers.cm2A,

    # vertical scattering in kf, keep "False" for normal TAS
    "kf_vert" : False,
}
