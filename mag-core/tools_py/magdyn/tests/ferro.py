#
# testing the lswt algorithm (https://arxiv.org/abs/1402.6069) for a 1d ferromagnetic chain
# @author Tobias Weber <tweber@ill.fr>
# @date 24-oct-2024
# @license GPLv3, see 'LICENSE' file
#

import numpy as np
import numpy.linalg as la
import matplotlib.pyplot as plt


# magnetic sites
sites = [
	{ "S" : 1., "Sdir" : [ 0, 0, 1 ] },
]

# magnetic couplings
couplings = [
	{ "sites" : [ 0, 0 ], "J" : -1., "DMI" : [ 0, 0, 0 ], "dist" : [ 1, 0, 0 ] },
]


# skew-symmetric matrix
def skew(vec):
	return np.array([
		[      0.,   vec[2],  -vec[1] ],
		[ -vec[2],       0.,   vec[0] ],
		[  vec[1],  -vec[0],       0. ] ])


# calculate spin rotations
for site in sites:
	zdir = np.array([ 0., 0., 1. ])
	Sdir = np.array(site["Sdir"]) / la.norm(site["Sdir"])
	axis = np.array([ 1., 0., 0. ])
	s = 0.

	if np.allclose(Sdir, zdir):
		# spin and z axis parallel
		c = 1.
	elif np.allclose(Sdir, -zdir):
		# spin and z axis anti-parallel
		c = -1.
	else:
		# sine and cosine of the angle between spin and z axis
		axis = np.cross(Sdir, zdir)
		s = la.norm(axis)
		c = np.dot(Sdir, zdir)
		axis /= s

	# rotation via rodrigues' formula, see (Arens 2015), p. 718 and p. 816
	rot = (1. - c) * np.outer(axis, axis) + np.diag([ c, c, c ]) - skew(axis)*s
	site["u"] = rot[0, :] + 1j  * rot[1, :]
	site["v"] = rot[2, :]

	#print(np.dot(rot, Sdir))
	#print("\nrot = \n%s\nu = %s\nv = %s" % (rot, site["u"], site["v"]))



# calculate real interaction matrices
for coupling in couplings:
	J = coupling["J"]
	coupling["J_real"] = np.diag([ J, J, J ]) + skew(coupling["DMI"])

	#print("\nJ_real =\n%s" % coupling["J_real"])


def get_energies(Qvec):
	#print("\n\nQ = %s" % Qvec)

	# fourier transform interaction matrices
	num_sites = len(sites)
	J_fourier = np.zeros((num_sites, num_sites, 3, 3), dtype = complex)
	J0_fourier = np.zeros((num_sites, num_sites, 3, 3), dtype = complex)

	for coupling in couplings:
		dist = np.array(coupling["dist"])
		J_real = coupling["J_real"]
		site1 = coupling["sites"][0]
		site2 = coupling["sites"][1]

		phase = -1j * 2.*np.pi * np.dot(dist, Qvec)

		J_fourier[site1, site2] += J_real * np.exp(phase)
		J_fourier[site2, site1] += J_real.transpose() * np.exp(-phase)
		J0_fourier[site1, site2] += J_real
		J0_fourier[site2, site1] += J_real.transpose()

	#print("\nJ_fourier =\n%s\n\nJ0_fourier =\n%s" % (J_fourier, J0_fourier))


	# hamiltonian
	H = np.zeros((2*num_sites, 2*num_sites), dtype = complex)

	for i in range(num_sites):
		S_i = sites[i]["S"]
		u_i = sites[i]["u"]
		v_i = sites[i]["v"]

		for j in range(num_sites):
			S_j = sites[j]["S"]
			u_j = sites[j]["u"]
			v_j = sites[j]["v"]
			S = 0.5 * np.sqrt(S_i * S_j)

			H[i, j] += \
				S * np.dot(u_i, np.dot(J_fourier[i][j], u_j.conj())) - \
				S_j * np.dot(v_i, np.dot(J0_fourier[i][j], v_j))
			H[num_sites + i, num_sites + j] += \
				S * np.dot(u_i.conj(), np.dot(J_fourier[i][j], u_j)) - \
				S_j * np.dot(v_i, np.dot(J0_fourier[i][j], v_j))
			H[i, num_sites + j] += \
				S * np.dot(u_i, np.dot(J_fourier[i][j], u_j))
			H[num_sites + i, j] += \
				(S * np.dot(u_j, np.dot(J_fourier[j][i], u_i))).conj()

	#print("\nH =\n%s" % H)


	# trafo
	C = la.cholesky(H)
	signs = np.diag(np.concatenate((np.repeat(1, num_sites), np.repeat(-1, num_sites))))
	H_trafo = np.dot(C.transpose().conj(), np.dot(signs, C))

	#print("\nC =\n%s\n\nH_trafo =\n%s" % (C, H_trafo))


	# the eigenvalues of H give the energies
	Es = np.real(la.eigvals(H_trafo))
	return Es


# plot
hs = []
Es = []
for h in np.linspace(-1, 1, 128):
	try:
		Qvec = np.array([ h, 0, 0 ])
		Es.append(get_energies(Qvec))
		hs.append(h)
	except la.LinAlgError:
		pass

plt.plot()
plt.xlabel("h (rlu)")
plt.ylabel("E (meV)")
plt.plot(hs, Es)
plt.show()