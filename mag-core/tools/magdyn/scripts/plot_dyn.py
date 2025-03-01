#
# plots dispersion curves
# @author Tobias Weber <tweber@ill.fr>
# @date 18-sep-2024
# @license GPLv3, see 'LICENSE' file
#
# ----------------------------------------------------------------------------
# mag-core (part of the Takin software suite)
# Copyright (C) 2018-2024  Tobias WEBER (Institut Laue-Langevin (ILL),
#                          Grenoble, France).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# ----------------------------------------------------------------------------
#

import sys
import numpy
import matplotlib.pyplot as pyplot
pyplot.rcParams.update({
	"font.sans-serif" : "DejaVu Sans",
	"font.family" : "sans-serif",
	"font.size" : 12,
})


# -----------------------------------------------------------------------------
# options
# -----------------------------------------------------------------------------
show_dividers  = False  # show vertical bars between dispersion branches
plot_file      = ""     # file to save plot to
only_pos_E     = True   # ignore magnon annihilation?

S_available    = True   # data has a structure factor column
S_filter_min   = 1e-5   # cutoff minimum spectral weight
S_scale        = 10     # weight scaling factor
S_clamp_min    = 0.1    # min. clamp factor
S_clamp_max    = 100.   # max. clamp factor

branch_labels  = None   # Q end point names
branch_colours = None   # branch colours

width_ratios   = None   # lengths from one dispersion point to the next

h_column       = 0      # h column index in data files
k_column       = 1      # k column index
l_column       = 2      # l column index
E_column       = 3      # E column index
S_column       = 4      # S column index
branch_column  = 5      # branch column index
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# plot the 1d dispersion curves
# -----------------------------------------------------------------------------
def plot_disp_1d(data):
	num_branches = len(data)

	(plt, axes) = pyplot.subplots(nrows = 1, ncols = num_branches,
		width_ratios = width_ratios, sharey = True)

	# in case there's only one sub-plot
	if type(axes) != numpy.ndarray:
		axes = [ axes ]

	# dispersion branch index (not energy branch)
	branch_idx = 0
	for data_row in data:
		data_h = data_row[h_column]
		data_k = data_row[k_column]
		data_l = data_row[l_column]
		data_E = data_row[E_column]
		if S_available:
			data_S = data_row[S_column]
		else:
			data_S = numpy.zeros(len(data_E))

		if only_pos_E:
			# ignore magnon annihilation
			data_h = numpy.array([ h for (h, E) in zip(data_h, data_E) if E >= 0. ])
			data_k = numpy.array([ k for (k, E) in zip(data_k, data_E) if E >= 0. ])
			data_l = numpy.array([ l for (l, E) in zip(data_l, data_E) if E >= 0. ])
			data_S = numpy.array([ S for (S, E) in zip(data_S, data_E) if E >= 0. ])
			data_E = numpy.array([ E for E in data_E if E >= 0. ])

		if S_available and S_filter_min >= 0.:
			# filter weights below cutoff
			data_h = numpy.array([ h for (h, S) in zip(data_h, data_S) if S >= S_filter_min ])
			data_k = numpy.array([ k for (k, S) in zip(data_k, data_S) if S >= S_filter_min ])
			data_l = numpy.array([ l for (l, S) in zip(data_l, data_S) if S >= S_filter_min ])
			data_E = numpy.array([ E for (E, S) in zip(data_E, data_S) if S >= S_filter_min ])
			data_S = numpy.array([ S for S in data_S if S >= S_filter_min ])

		# branch start and end point
		b1 = ( data_h[0], data_k[0], data_l[0] )
		b2 = ( data_h[-1], data_k[-1], data_l[-1] )

		# find scan axis
		Q_diff = [
			numpy.abs(b1[0] - b2[0]),
			numpy.abs(b1[1] - b2[1]),
			numpy.abs(b1[2] - b2[2]) ]

		plot_idx = 0
		data_x = data_h
		if Q_diff[1] > Q_diff[plot_idx]:
			plot_idx = 1
			data_x = data_k
		elif Q_diff[2] > Q_diff[plot_idx]:
			plot_idx = 2
			data_x = data_l

		# ticks and labels
		axes[branch_idx].set_xlim(data_x[0], data_x[-1])

		if branch_colours != None and len(branch_colours) != 0:
			axes[branch_idx].set_facecolor(branch_colours[branch_idx])

		if branch_labels != None and len(branch_labels) != 0:
			tick_labels = [
				branch_labels[branch_idx],
				branch_labels[branch_idx + 1] ]
		else:
			tick_labels = [
				"(%.4g %.4g %.4g)" % (b1[0], b1[1], b1[2]),
				"(%.4g %.4g %.4g)" % (b2[0], b2[1], b2[2]) ]

		if branch_idx == 0:
			axes[branch_idx].set_ylabel("E (meV)")
		else:
			axes[branch_idx].get_yaxis().set_visible(False)
			if not show_dividers:
				axes[branch_idx].spines["left"].set_visible(False)

			tick_labels[0] = ""

		if not show_dividers and branch_idx != num_branches - 1:
			axes[branch_idx].spines["right"].set_visible(False)

		axes[branch_idx].set_xticks([data_x[0], data_x[-1]], labels = tick_labels)

		if branch_idx == num_branches / 2 - 1:
			axes[branch_idx].set_xlabel("Q (rlu)")

		# scale and clamp S
		data_S *= S_scale
		if S_clamp_min < S_clamp_max:
			data_S = numpy.clip(data_S, a_min = S_clamp_min, a_max = S_clamp_max)

		# plot the dispersion branch
		axes[branch_idx].scatter(data_x, data_E, marker = '.', s = data_S)

		branch_idx += 1

	plt.tight_layout()
	plt.subplots_adjust(wspace = 0)

	if plot_file != "":
		pyplot.savefig(plot_file)
	pyplot.show()
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# plot the 2d dispersion surfaces
# -----------------------------------------------------------------------------
def plot_disp_2d(data, Q_idx1 = -1, Q_idx2 = -1):
	data = data[0].T  # TODO: also use other files' data if given

	# find x and y axis to plot
	if Q_idx1 < 0 or Q_idx2 < 0:
		# Q start and end points
		b1 = ( data[:, h_column][0],  data[:, k_column][0],  data[:, l_column][0] )
		b2 = ( data[:, h_column][-1], data[:, k_column][-1], data[:, l_column][-1] )

		# find scan axis
		Q_diff = [
			numpy.abs(b1[0] - b2[0]),
			numpy.abs(b1[1] - b2[1]),
			numpy.abs(b1[2] - b2[2]) ]

		# first scan axis
		if Q_idx1 < 0:
			if Q_diff[1] > 0. and Q_idx2 != 0:
				Q_idx1 = 0
			if Q_diff[1] > Q_diff[Q_idx1] and Q_idx2 != 1:
				Q_idx1 = 1
			elif Q_diff[2] > Q_diff[Q_idx1] and Q_idx2 != 2:
				Q_idx1 = 2

		# second scan axis
		if Q_idx2 < 0:
			if Q_diff[1] > 0. and Q_idx1 != 0:
				Q_idx2 = 0
			if Q_diff[1] > Q_diff[Q_idx2] and Q_idx1 != 1:
				Q_idx2 = 1
			elif Q_diff[2] > Q_diff[Q_idx2] and Q_idx1 != 2:
				Q_idx2 = 2

	labels = [ "h (rlu)", "k (rlu)", "l (rlu)" ]

	(plt, axis) = pyplot.subplots(nrows = 1, ncols = 1,
		width_ratios = width_ratios, sharey = True,
		subplot_kw = { "projection" : "3d" })

	E_branch_idx = 0
	E_branch_max = int(numpy.max(data[:, branch_column]))

	# iterate energy branches
	for E_branch_idx in range(0, E_branch_max + 1):
		# filter data for given branch
		data_Q = [
			[ row[h_column + Q_idx1] for row in data if row[branch_column] == E_branch_idx ],
			[ row[h_column + Q_idx2] for row in data if row[branch_column] == E_branch_idx ]
		]
		data_E = [ row[E_column] for row in data if row[branch_column] == E_branch_idx ]
		if S_available:
			data_S = [ row[S_column] for row in data if row[branch_column] == E_branch_idx ]

		if only_pos_E:
			# ignore magnon annihilation
			data_Q[0] = [ Q for (Q, E) in zip(data_Q[0], data_E) if E >= 0. ]
			data_Q[1] = [ Q for (Q, E) in zip(data_Q[1], data_E) if E >= 0. ]
			if S_available:
				data_S = [ S for (S, E) in zip(data_S, data_E) if E >= 0. ]
			data_E = [ E for E in data_E if E >= 0. ]

		if S_available and S_filter_min >= 0.:
			# filter weights below cutoff
			data_Q[0] = [ Q for (Q, S) in zip(data_Q[0], data_S) if S >= S_filter_min ]
			data_Q[1] = [ Q for (Q, S) in zip(data_Q[1], data_S) if S >= S_filter_min ]
			data_E = [ E for (E, S) in zip(data_E, data_S) if S >= S_filter_min ]
			data_S = [ S for S in data_S if S >= S_filter_min ]

		if(len(data_E) < 1):
			continue

		# choose branch colour
		r = int(0xff - 0xff * (E_branch_idx / E_branch_max))
		b = int(0x00 + 0xff * (E_branch_idx / E_branch_max))

		axis.plot_trisurf(data_Q[0], data_Q[1], data_E,
			color = "#%02x00%02xaf" % (r, b),
			antialiased = True)

	axis.set_xlabel(labels[Q_idx1])
	axis.set_ylabel(labels[Q_idx2])
	axis.set_zlabel("E (meV)")

	plt.tight_layout()
	plt.subplots_adjust(wspace = 0)

	if plot_file != "":
		pyplot.savefig(plot_file)
	pyplot.show()
# -----------------------------------------------------------------------------


if __name__ == "__main__":
	if len(sys.argv) < 2:
		print("Please specify data file names.")
		print("Options:")
		print("\t--2d    plot 2d dispersion surfaces")
		sys.exit(-1)

	data = []
	plot_2d = False
	Q_idx1 = -1  # first axis to plot, -1: automatically search
	Q_idx2 = -1  # second axis to plot, -1: automatically search

	# iterate arguments/filenames
	for arg in sys.argv[1:]:
		if arg == "--2d":
			plot_2d = True
			continue
		elif arg == "--noS":
			S_available = False
			branch_column -= 1
			continue
		elif arg == "--allE":
			only_pos_E = False
			continue
		data.append(numpy.loadtxt(arg).T)

	if plot_2d:
		plot_disp_2d(data, Q_idx1, Q_idx2)
	else:
		plot_disp_1d(data)
