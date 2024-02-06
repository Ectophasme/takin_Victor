/**
 * magnetic dynamics -- calculations for sites and coupling terms
 * @author Tobias Weber <tweber@ill.fr>
 * @date Jan-2022
 * @license GPLv3, see 'LICENSE' file
 * @desc The present version was forked on 28-Dec-2018 from my privately developed "misc" project (https://github.com/t-weber/misc).
 *
 * ----------------------------------------------------------------------------
 * mag-core (part of the Takin software suite)
 * Copyright (C) 2018-2024  Tobias WEBER (Institut Laue-Langevin (ILL),
 *                          Grenoble, France).
 * "misc" project
 * Copyright (C) 2017-2022  Tobias WEBER (privately developed).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 */

#include "magdyn.h"
#include <QtWidgets/QMessageBox>

#include <iostream>
#include <boost/scope_exit.hpp>



/**
 * flip the coordinates of the magnetic site positions
 * (e.g. to get the negative phase factor for weights)
 */
void MagDynDlg::MirrorAtoms()
{
	BOOST_SCOPE_EXIT(this_)
	{
		this_->m_ignoreCalc = false;
		if(this_->m_autocalc->isChecked())
			this_->CalcAll();
	} BOOST_SCOPE_EXIT_END
	m_ignoreCalc = true;

	// iterate the magnetic sites
	for(int row=0; row<m_sitestab->rowCount(); ++row)
	{
		auto *pos_x = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_sitestab->item(row, COL_SITE_POS_X));
		auto *pos_y = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_sitestab->item(row, COL_SITE_POS_Y));
		auto *pos_z = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_sitestab->item(row, COL_SITE_POS_Z));

		if(!pos_x || !pos_y || !pos_z)
		{
			std::cerr << "Invalid entry in sites table row "
				<< row << "." << std::endl;
			continue;
		}

		pos_x->SetValue(-pos_x->GetValue());
		pos_y->SetValue(-pos_y->GetValue());
		pos_z->SetValue(-pos_z->GetValue());
	}
}



/**
 * rotate the direction of the magnetic field
 */
void MagDynDlg::RotateField(bool ccw)
{
	t_vec_real axis = tl2::create<t_vec_real>(
	{
		(t_real)m_rot_axis[0]->value(),
		(t_real)m_rot_axis[1]->value(),
		(t_real)m_rot_axis[2]->value(),
	});

	t_vec_real B = tl2::create<t_vec_real>(
	{
		(t_real)m_field_dir[0]->value(),
		(t_real)m_field_dir[1]->value(),
		(t_real)m_field_dir[2]->value(),
	});

	t_real angle = m_rot_angle->value() / 180.*tl2::pi<t_real>;
	if(!ccw)
		angle = -angle;

	t_mat_real R = tl2::rotation<t_mat_real, t_vec_real>(
		axis, angle, false);
	B = R*B;
	tl2::set_eps_0(B, g_eps);

	for(int i=0; i<3; ++i)
	{
		m_field_dir[i]->blockSignals(true);
		m_field_dir[i]->setValue(B[i]);
		m_field_dir[i]->blockSignals(false);
	}

	if(m_autocalc->isChecked())
		CalcAll();
};



/**
 * set selected field as current
 */
void MagDynDlg::SetCurrentField()
{
	if(m_fields_cursor_row < 0 || m_fields_cursor_row >= m_fieldstab->rowCount())
		return;

	const auto* Bh = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
		m_fieldstab->item(m_fields_cursor_row, COL_FIELD_H));
	const auto* Bk = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
		m_fieldstab->item(m_fields_cursor_row, COL_FIELD_K));
	const auto* Bl = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
		m_fieldstab->item(m_fields_cursor_row, COL_FIELD_L));
	const auto* Bmag = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
		m_fieldstab->item(m_fields_cursor_row, COL_FIELD_MAG));

	if(!Bh || !Bk || !Bl || !Bmag)
		return;

	m_ignoreCalc = true;
	BOOST_SCOPE_EXIT(this_)
	{
		this_->m_ignoreCalc = false;
		if(this_->m_autocalc->isChecked())
			this_->CalcAll();
	} BOOST_SCOPE_EXIT_END

	m_field_dir[0]->setValue(Bh->GetValue());
	m_field_dir[1]->setValue(Bk->GetValue());
	m_field_dir[2]->setValue(Bl->GetValue());
	m_field_mag->setValue(Bmag->GetValue());
}



/**
 * generate magnetic sites form the space group symmetries
 */
void MagDynDlg::GenerateSitesFromSG()
{
	BOOST_SCOPE_EXIT(this_)
	{
		this_->m_ignoreCalc = false;
		if(this_->m_autocalc->isChecked())
			this_->CalcAll();
	} BOOST_SCOPE_EXIT_END
	m_ignoreCalc = true;

	try
	{
		// symops of current space group
		auto sgidx = m_comboSG->itemData(m_comboSG->currentIndex()).toInt();
		if(sgidx < 0 || t_size(sgidx) >= m_SGops.size())
		{
			QMessageBox::critical(this, "Magnetic Dynamics",
				"Invalid space group selected.");
			return;
		}

		const auto& ops = m_SGops[sgidx];
		std::vector<std::tuple<
			std::string,                           // 0: name
			t_real, t_real, t_real,                // 1-3: position
			std::string, std::string, std::string, // 4-6: spin direction
			t_real,                                // 7: spin magnitude
			std::string, std::string, std::string, // 8-10: spin orthogonal plane
			std::string                            // 11: colour
			>> generatedsites;

		// avoids multiple occupation of the same site
		auto remove_duplicate_sites = [&generatedsites]()
		{
			for(auto iter1 = generatedsites.begin(); iter1 != generatedsites.end(); ++iter1)
			{
				for(auto iter2 = std::next(iter1, 1); iter2 != generatedsites.end();)
				{
					bool same_x = tl2::equals<t_real>(std::get<1>(*iter1), std::get<1>(*iter2), g_eps);
					bool same_y = tl2::equals<t_real>(std::get<2>(*iter1), std::get<2>(*iter2), g_eps);
					bool same_z = tl2::equals<t_real>(std::get<3>(*iter1), std::get<3>(*iter2), g_eps);

					if(same_x && same_y && same_z)
						iter2 = generatedsites.erase(iter2);
					else
						++iter2;
				}
			}
		};

		// if no previous positions are available, symmetrise based on the (000) position
		if(!m_sitestab->rowCount())
		{
			t_real x = 0, y = 0, z = 1;
			std::string sx = "0", sy = "0", sz = "1";
			t_real S = 1;
			AddSiteTabItem(-1, "pos", x, y, z, sx, sy, sz, S);
		}

		// iterate and symmetrise existing sites
		for(int row=0; row<m_sitestab->rowCount(); ++row)
		{
			std::string ident = m_sitestab->item(row, COL_SITE_NAME)->text().toStdString();

			t_real x = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_sitestab->item(row, COL_SITE_POS_X))->GetValue();
			t_real y = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_sitestab->item(row, COL_SITE_POS_Y))->GetValue();
			t_real z = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_sitestab->item(row, COL_SITE_POS_Z))->GetValue();

			std::string sx = m_sitestab->item(row, COL_SITE_SPIN_X)->text().toStdString();
			std::string sy = m_sitestab->item(row, COL_SITE_SPIN_Y)->text().toStdString();
			std::string sz = m_sitestab->item(row, COL_SITE_SPIN_Z)->text().toStdString();

			std::string sox = "auto";
			std::string soy = "auto";
			std::string soz = "auto";
			if(m_allow_ortho_spin)
			{
				sox = m_sitestab->item(row, COL_SITE_SPIN_ORTHO_X)->text().toStdString();
				soy = m_sitestab->item(row, COL_SITE_SPIN_ORTHO_Y)->text().toStdString();
				soz = m_sitestab->item(row, COL_SITE_SPIN_ORTHO_Z)->text().toStdString();
			}

			t_real S = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_sitestab->item(row, COL_SITE_SPIN_MAG))->GetValue();

			t_vec_real sitepos = tl2::create<t_vec_real>({x, y, z, 1});
			auto newsitepos = tl2::apply_ops_hom<t_vec_real, t_mat_real, t_real>(
				sitepos, ops, g_eps);

			std::string rgb = m_sitestab->item(row, COL_SITE_RGB)->text().toStdString();

			for(t_size newsite_idx=0; newsite_idx<newsitepos.size(); ++newsite_idx)
			{
				const auto& newsite = newsitepos[newsite_idx];

				generatedsites.emplace_back(std::make_tuple(
					ident + "_" + tl2::var_to_str(newsite_idx),
					newsite[0], newsite[1], newsite[2],
					sx, sy, sz, S, sox, soy, soz, rgb));
			}

			remove_duplicate_sites();
		}

		// remove original sites
		DelTabItem(m_sitestab, -1);

		// add new sites
		for(const auto& site : generatedsites)
		{
			std::apply(&MagDynDlg::AddSiteTabItem,
				std::tuple_cat(std::make_tuple(this, -1), site));
		}
	}
	catch(const std::exception& ex)
	{
		QMessageBox::critical(this, "Magnetic Dynamics", ex.what());
	}
}



/**
 * generate exchange terms from space group symmetries
 */
void MagDynDlg::GenerateCouplingsFromSG()
{
	BOOST_SCOPE_EXIT(this_)
	{
		this_->m_ignoreCalc = false;
		if(this_->m_autocalc->isChecked())
			this_->CalcAll();
	} BOOST_SCOPE_EXIT_END
	m_ignoreCalc = true;

	try
	{
		// symops of current space group
		auto sgidx = m_comboSG2->itemData(m_comboSG2->currentIndex()).toInt();
		if(sgidx < 0 || t_size(sgidx) >= m_SGops.size())
		{
			QMessageBox::critical(this, "Magnetic Dynamics", "Invalid space group selected.");
			return;
		}

		std::vector<std::tuple<
			std::string,                           // ident
			t_size, t_size,                        // uc magnetic site indices
			t_real, t_real, t_real,                // supercell vector
			std::string,                           // exchange term (not modified)
			std::string, std::string, std::string, // dmi vector
			std::string, std::string, std::string, //
			std::string, std::string, std::string, // general exchange matrix
			std::string, std::string, std::string, //
			std::string                            // colour
			>> generatedcouplings;

		// avoids duplicate coupling terms
		auto remove_duplicate_terms = [&generatedcouplings]()
		{
			for(auto iter1 = generatedcouplings.begin(); iter1 != generatedcouplings.end(); ++iter1)
			{
				for(auto iter2 = std::next(iter1, 1); iter2 != generatedcouplings.end();)
				{
					bool same_uc_idx1 = (std::get<1>(*iter1) == std::get<1>(*iter2));
					bool same_uc_idx2 = (std::get<2>(*iter1) == std::get<2>(*iter2));

					bool same_sc_x = tl2::equals<t_real>(std::get<3>(*iter1), std::get<3>(*iter2), g_eps);
					bool same_sc_y = tl2::equals<t_real>(std::get<4>(*iter1), std::get<4>(*iter2), g_eps);
					bool same_sc_z = tl2::equals<t_real>(std::get<5>(*iter1), std::get<5>(*iter2), g_eps);

					if(same_uc_idx1 && same_uc_idx2 && same_sc_x && same_sc_y && same_sc_z)
						iter2 = generatedcouplings.erase(iter2);
					else
						++iter2;
				}
			}
		};

		const auto& sites = m_dyn.GetMagneticSites();
		const auto& ops = m_SGops[sgidx];

		// get all site positions in unit cell in homogeneous coordinates
		std::vector<t_vec_real> sites_uc;
		sites_uc.reserve(sites.size());
		for(const auto& site : sites)
			sites_uc.push_back(tl2::create<t_vec_real>({
				site.pos[0], site.pos[1], site.pos[2], 1. }));

		// if no previous coupling terms are available, add default ones
		if(!m_termstab->rowCount())
		{
			AddTermTabItem(-1, "term",
				0, 1,                             // magnetic site indices
				t_real(0), t_real(0), t_real(0),  // dist
				"-1",                             // J
				"0", "0", "0.1");                 // dmi
		}

		tl2::ExprParser parser = m_dyn.GetExprParser();

		// iterate existing coupling terms
		for(int row=0; row<m_termstab->rowCount(); ++row)
		{
			std::string ident = m_termstab->item(row, COL_XCH_NAME)->text().toStdString();

			t_real sc_x = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_termstab->item(row, COL_XCH_DIST_X))->GetValue();
			t_real sc_y = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_termstab->item(row, COL_XCH_DIST_Y))->GetValue();
			t_real sc_z = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_termstab->item(row, COL_XCH_DIST_Z))->GetValue();

			// have to evaluate the dmi vector here, because we can't transform its expression symbolically yet
			bool dmi_x_ok = parser.parse(
				m_termstab->item(row, COL_XCH_DMI_X)->text().toStdString());
			t_real dmi_x = parser.eval().real();
			bool dmi_y_ok = parser.parse(
				m_termstab->item(row, COL_XCH_DMI_Y)->text().toStdString());
			t_real dmi_y = parser.eval().real();
			bool dmi_z_ok = parser.parse(
				m_termstab->item(row, COL_XCH_DMI_Z)->text().toStdString());
			t_real dmi_z = parser.eval().real();

			if(!dmi_x_ok || !dmi_y_ok || !dmi_z_ok)
				std::cerr << "Could not parse DMI vector expression." << std::endl;

			// have to evaluate the general interaction matrix here, because we can't transform its expression symbolically yet
			t_real genJ_xx = 0, genJ_xy = 0, genJ_xz = 0;
			t_real genJ_yx = 0, genJ_yy = 0, genJ_yz = 0;
			t_real genJ_zx = 0, genJ_zy = 0, genJ_zz = 0;
			if(m_allow_general_J)
			{
				bool genJ_xx_ok = parser.parse(
					m_termstab->item(row, COL_XCH_GEN_XX)->text().toStdString());
				genJ_xx = parser.eval().real();
				bool genJ_xy_ok = parser.parse(
					m_termstab->item(row, COL_XCH_GEN_XY)->text().toStdString());
				genJ_xy = parser.eval().real();
				bool genJ_xz_ok = parser.parse(
					m_termstab->item(row, COL_XCH_GEN_XZ)->text().toStdString());
				genJ_xz = parser.eval().real();
				bool genJ_yx_ok = parser.parse(
					m_termstab->item(row, COL_XCH_GEN_YX)->text().toStdString());
				genJ_yx = parser.eval().real();
				bool genJ_yy_ok = parser.parse(
					m_termstab->item(row, COL_XCH_GEN_YY)->text().toStdString());
				genJ_yy = parser.eval().real();
				bool genJ_yz_ok = parser.parse(
					m_termstab->item(row, COL_XCH_GEN_YZ)->text().toStdString());
				genJ_yz = parser.eval().real();
				bool genJ_zx_ok = parser.parse(
					m_termstab->item(row, COL_XCH_GEN_ZX)->text().toStdString());
				genJ_zx = parser.eval().real();
				bool genJ_zy_ok = parser.parse(
					m_termstab->item(row, COL_XCH_GEN_ZY)->text().toStdString());
				genJ_zy = parser.eval().real();
				bool genJ_zz_ok = parser.parse(
					m_termstab->item(row, COL_XCH_GEN_ZZ)->text().toStdString());
				genJ_zz = parser.eval().real();

				if(!genJ_xx_ok || !genJ_xy_ok || !genJ_xz_ok ||
					!genJ_yx_ok || !genJ_yy_ok || !genJ_yz_ok ||
					!genJ_zx_ok || !genJ_zy_ok || !genJ_zz_ok)
					std::cerr << "Could not parse general interaction matrix expression."
						<< std::endl;
			}

			std::string rgb = m_termstab->item(row, COL_XCH_RGB)->text().toStdString();
			std::string oldJ = m_termstab->item(row, COL_XCH_INTERACTION)->text().toStdString();

			auto atom_1_idx = GetTermAtomIndex(row, 0);
			auto atom_2_idx = GetTermAtomIndex(row, 1);

			if(!atom_1_idx || !atom_2_idx)
			{
				std::cerr << "Site indices for term " << row << " (\""
					<< ident << "\") are invalid, skipping." << std::endl;
				continue;
			}

			// magnetic site positions in unit cell
			if(*atom_1_idx >= sites_uc.size() || *atom_2_idx >= sites_uc.size())
			{
				std::cerr << "Site indices for term " << row << " (\""
					<< ident << "\") are out of bounds, skipping." << std::endl;
				continue;
			}
			const t_vec_real& site1 = sites_uc[*atom_1_idx];
			t_vec_real site2 = sites_uc[*atom_2_idx];

			// position in super cell
			site2 += tl2::create<t_vec_real>({ sc_x, sc_y, sc_z, 0. });

			// generate new (possibly supercell) sites with symop
			auto sites1_sc = tl2::apply_ops_hom<t_vec_real, t_mat_real, t_real>(
				site1, ops, g_eps, false /*keep in uc*/, true /*ignore occupied*/, true);
			auto sites2_sc = tl2::apply_ops_hom<t_vec_real, t_mat_real, t_real>(
				site2, ops, g_eps, false /*keep in uc*/, true /*ignore occupied*/, true);

			// generate dmi vectors
			t_vec_real dmi = tl2::create<t_vec_real>({dmi_x, dmi_y, dmi_z, 0});
			auto newdmis = tl2::apply_ops_hom<t_vec_real, t_mat_real, t_real>(
				dmi, ops, g_eps, false, true);

			// generate general J matrices
			t_mat_real Jgen = tl2::create<t_mat_real>({
				genJ_xx, genJ_xy, genJ_xz,  0,
				genJ_yx, genJ_yy, genJ_yz,  0,
				genJ_zx, genJ_zy, genJ_zz,  0,
				0,             0,       0,  0
			});
			auto newJgens = tl2::apply_ops_hom<t_mat_real, t_real>(Jgen, ops);

			for(t_size op_idx=0; op_idx<sites1_sc.size(); ++op_idx)
			{
				const t_vec_real& site1_sc = sites1_sc[op_idx];
				const t_vec_real& site2_sc = sites2_sc[op_idx];
				const t_vec_real& newdmi = newdmis[op_idx];
				const t_mat_real& newJgen = newJgens[op_idx];

				// get position of the site in the supercell
				auto [sc1_ok, site1_sc_idx, sc1] = tl2::get_supercell(site1_sc, sites_uc, 3, g_eps);
				auto [sc2_ok, site2_sc_idx, sc2] = tl2::get_supercell(site2_sc, sites_uc, 3, g_eps);
				t_vec_real sc_dist = sc2 - sc1;

				if(!sc1_ok || !sc2_ok)
				{
					std::cerr << "Could not find supercell for position generated from symop "
						<< op_idx << "." << std::endl;
				}

				generatedcouplings.emplace_back(std::make_tuple(
					ident + "_" + tl2::var_to_str(op_idx), site1_sc_idx, site2_sc_idx,
					sc_dist[0], sc_dist[1], sc_dist[2], oldJ,
					tl2::var_to_str(newdmi[0]), tl2::var_to_str(newdmi[1]), tl2::var_to_str(newdmi[2]),
					tl2::var_to_str(newJgen(0,0)), tl2::var_to_str(newJgen(0,1)), tl2::var_to_str(newJgen(0,2)),
					tl2::var_to_str(newJgen(1,0)), tl2::var_to_str(newJgen(1,1)), tl2::var_to_str(newJgen(1,2)),
					tl2::var_to_str(newJgen(2,0)), tl2::var_to_str(newJgen(2,1)), tl2::var_to_str(newJgen(2,2)),
					rgb));
			}

			remove_duplicate_terms();
		}

		if(!generatedcouplings.size())
		{
			QMessageBox::critical(this, "Magnetic Dynamics", "No couplings could be generated.");
			return;
		}

		// remove original couplings
		DelTabItem(m_termstab, -1);

		// add new couplings
		for(const auto& coupling : generatedcouplings)
		{
			std::apply(&MagDynDlg::AddTermTabItem,
				std::tuple_cat(std::make_tuple(this, -1), coupling));
		}
	}
	catch(const std::exception& ex)
	{
		QMessageBox::critical(this, "Magnetic Dynamics", ex.what());
	}
}



/**
 * helper struct for finding possible couplings
 */
struct PossibleCoupling
{
	// corresponding unit cell position
	t_vec_real pos1_uc{};
	t_vec_real pos2_uc{};

	// magnetic site position in supercell
	t_vec_real sc_vec{};
	t_vec_real pos2_sc{};

	// coordinates in orthogonal lab units
	t_vec_real pos1_uc_lab{};
	t_vec_real pos2_sc_lab{};

	// corresponding unit cell index
	t_size idx1_uc{};
	t_size idx2_uc{};

	// distance between the two magnetic sites
	t_real dist{};
};



/**
 * generate possible couplings up to a certain distance
 */
void MagDynDlg::GeneratePossibleCouplings()
{
	BOOST_SCOPE_EXIT(this_)
	{
		this_->m_ignoreCalc = false;
		if(this_->m_autocalc->isChecked())
			this_->CalcAll();
	} BOOST_SCOPE_EXIT_END
	m_ignoreCalc = true;

	try
	{
		t_real maxdist = m_maxdist->value();
		t_real sc_max = t_real(m_maxSC->value());
		t_size max_couplings = m_maxcouplings->value();

		t_real a = m_xtallattice[0]->value();
		t_real b = m_xtallattice[1]->value();
		t_real c = m_xtallattice[2]->value();
		t_real alpha = m_xtalangles[0]->value() / 180. * tl2::pi<t_real>;
		t_real beta = m_xtalangles[1]->value()/ 180. * tl2::pi<t_real>;
		t_real gamma = m_xtalangles[2]->value()/ 180. * tl2::pi<t_real>;
		t_mat_real A = tl2::A_matrix<t_mat_real>(a, b, c, alpha, beta, gamma);

		std::vector<PossibleCoupling> couplings;
		const auto& sites = m_dyn.GetMagneticSites();

		// get all site position vectors in unit cell
		std::vector<t_vec_real> sites_uc;
		sites_uc.reserve(sites.size());
		for(const auto& site : sites)
			sites_uc.push_back(tl2::create<t_vec_real>({
				site.pos[0], site.pos[1], site.pos[2], }));

		// generate a list of supercell vectors
		std::vector<t_vec_real> sc_vecs;
		sc_vecs.reserve(sc_max * sc_max * sc_max * 2*2*2);
		for(t_real sc_h = -sc_max; sc_h<=sc_max; sc_h += 1.)
		{
			for(t_real sc_k = -sc_max; sc_k<=sc_max; sc_k += 1.)
			{
				for(t_real sc_l = -sc_max; sc_l<=sc_max; sc_l += 1.)
				{
					sc_vecs.emplace_back(
						tl2::create<t_vec_real>({ sc_h, sc_k, sc_l }));
				}
			}
		}

		for(const t_vec_real& sc_vec : sc_vecs)
		{
			for(t_size idx1=0; idx1<sites.size()-1; ++idx1)
			{
				for(t_size idx2=idx1+1; idx2<sites.size(); ++idx2)
				{
					PossibleCoupling coupling;

					coupling.idx1_uc = idx1;
					coupling.idx2_uc = idx2;

					coupling.pos1_uc = sites_uc[idx1];
					coupling.pos2_uc = sites_uc[idx2];

					coupling.sc_vec = sc_vec;
					coupling.pos2_sc = coupling.pos2_uc + sc_vec;

					// transform to lab units for correct distance
					coupling.pos1_uc_lab = A * coupling.pos1_uc;
					coupling.pos2_sc_lab = A * coupling.pos2_sc;

					coupling.dist = tl2::norm<t_vec_real>(
						coupling.pos2_sc_lab - coupling.pos1_uc_lab);
					if(coupling.dist <= maxdist)
						couplings.emplace_back(std::move(coupling));
				}
			}
		}

		// sort couplings by distance
		std::stable_sort(couplings.begin(), couplings.end(),
			[](const PossibleCoupling& coupling1, const PossibleCoupling& coupling2) -> bool
		{
			return coupling1.dist < coupling2.dist;
		});

		// add couplings to list
		t_size coupling_idx = 0;
		for(const PossibleCoupling& coupling : couplings)
		{
			if(coupling_idx >= max_couplings)
				break;

			std::ostringstream ident;
			ident << "coupling_" << (coupling_idx + 1);
			auto generatedcoupling = std::make_tuple(
				ident.str(),
				coupling.idx1_uc, coupling.idx2_uc,
				coupling.sc_vec[0], coupling.sc_vec[1], coupling.sc_vec[2],
				"0",                                   // exchange term
				"0", "0", "0",                         // dmi vector
				"0", "0", "0",                         //
				"0", "0", "0",                         // general exchange matrix
				"0", "0", "0",                         //
				"auto");                               // colour

			++coupling_idx;

			std::apply(&MagDynDlg::AddTermTabItem,
				std::tuple_cat(std::make_tuple(this, -1), generatedcoupling));
		}
	}
	catch(const std::exception& ex)
	{
		QMessageBox::critical(this, "Magnetic Dynamics", ex.what());
	}
}



std::optional<t_size> MagDynDlg::GetTermAtomIndex(int row, int num) const
{
	int col = num==0 ? COL_XCH_ATOM1_IDX : COL_XCH_ATOM2_IDX;

	auto *atom_idx = static_cast<tl2::NumericTableWidgetItem<t_size>*>(
		m_termstab->item(row, col));

	std::optional<t_size> idx = std::nullopt;

	// try to find the magnetic site with the given name (if it's unique)
	std::string atomName = atom_idx->text().toStdString();
	if(auto sites = m_dyn.FindMagneticSites(atomName); sites.size() == 1)
	{
		//std::cout << sites[0]->name<< " -> " << sites[0]->index << std::endl;
		idx = sites[0]->index;
	}

	// otherwise try to use the given index
	bool valid = true;
	if(!idx)
	{
		idx = atom_idx->GetValue(&valid);
		if(!valid)
			idx = std::nullopt;
	}

	// out-of-bounds check for atom index
	if(!idx || *idx >= m_dyn.GetMagneticSites().size() || !valid)
	{
		// set background red
		atom_idx->setBackground(QBrush(QColor(0xff, 0x00, 0x00)));
	}
	else
	{
		// restore background
		QBrush brush = m_termstab->item(row, COL_XCH_NAME)->background();
		atom_idx->setBackground(brush);
	}

	return idx;
}



/**
 * get the sites, exchange terms, and variables from the table
 * and transfer them to the dynamics calculator
 */
void MagDynDlg::SyncSitesAndTerms()
{
	if(m_ignoreCalc)
		return;
	m_dyn.Clear();

	BOOST_SCOPE_EXIT(this_)
	{
		this_->m_sitestab->blockSignals(false);
		this_->m_termstab->blockSignals(false);
		this_->m_varstab->blockSignals(false);
	} BOOST_SCOPE_EXIT_END
	m_sitestab->blockSignals(true);
	m_termstab->blockSignals(true);
	m_varstab->blockSignals(true);

	// get ordering vector and rotation axis
	{
		t_vec_real ordering = tl2::create<t_vec_real>(
		{
			(t_real)m_ordering[0]->value(),
			(t_real)m_ordering[1]->value(),
			(t_real)m_ordering[2]->value(),
		});

		t_vec_real rotaxis = tl2::create<t_vec_real>(
		{
			(t_real)m_normaxis[0]->value(),
			(t_real)m_normaxis[1]->value(),
			(t_real)m_normaxis[2]->value(),
		});

		m_dyn.SetOrderingWavevector(ordering);
		m_dyn.SetRotationAxis(rotaxis);
	}

	// get external field
	if(m_use_field->isChecked())
	{
		t_magdyn::ExternalField field;
		field.dir = tl2::create<t_vec_real>(
		{
			(t_real)m_field_dir[0]->value(),
			(t_real)m_field_dir[1]->value(),
			(t_real)m_field_dir[2]->value(),
		});

		field.mag = m_field_mag->value();
		field.align_spins = m_align_spins->isChecked();

		m_dyn.SetExternalField(field);
	}

	// get temperature
	if(m_use_temperature->isChecked())
	{
		t_real temp = m_temperature->value();
		m_dyn.SetTemperature(temp);
	}

	// get variables
	for(int row=0; row<m_varstab->rowCount(); ++row)
	{
		auto *name = m_varstab->item(row, COL_VARS_NAME);
		auto *val_re = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_varstab->item(row, COL_VARS_VALUE_REAL));
		auto *val_im = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_varstab->item(row, COL_VARS_VALUE_IMAG));

		if(!name || !val_re || !val_im)
		{
			std::cerr << "Invalid entry in variables table row "
				<< row << "." << std::endl;
			continue;
		}

		t_magdyn::Variable var;
		var.name = name->text().toStdString();
		var.value = val_re->GetValue() + val_im->GetValue() * t_cplx(0, 1);

		m_dyn.AddVariable(std::move(var));
	}

	// get magnetic sites
	for(int row=0; row<m_sitestab->rowCount(); ++row)
	{
		auto *name = m_sitestab->item(row, COL_SITE_NAME);
		auto *pos_x = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_sitestab->item(row, COL_SITE_POS_X));
		auto *pos_y = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_sitestab->item(row, COL_SITE_POS_Y));
		auto *pos_z = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_sitestab->item(row, COL_SITE_POS_Z));
		auto *spin_x = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_sitestab->item(row, COL_SITE_SPIN_X));
		auto *spin_y = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_sitestab->item(row, COL_SITE_SPIN_Y));
		auto *spin_z = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_sitestab->item(row, COL_SITE_SPIN_Z));
		auto *spin_mag = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_sitestab->item(row, COL_SITE_SPIN_MAG));

		tl2::NumericTableWidgetItem<t_real> *spin_ortho_x = nullptr;
		tl2::NumericTableWidgetItem<t_real> *spin_ortho_y = nullptr;
		tl2::NumericTableWidgetItem<t_real> *spin_ortho_z = nullptr;
		if(m_allow_ortho_spin)
		{
			spin_ortho_x = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_sitestab->item(row, COL_SITE_SPIN_ORTHO_X));
			spin_ortho_y = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_sitestab->item(row, COL_SITE_SPIN_ORTHO_Y));
			spin_ortho_z = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_sitestab->item(row, COL_SITE_SPIN_ORTHO_Z));
		}

		if(!name || !pos_x || !pos_y || !pos_z ||
			!spin_x || !spin_y || !spin_z || !spin_mag)
		{
			std::cerr << "Invalid entry in sites table row "
				<< row << "." << std::endl;
			continue;
		}

		t_magdyn::MagneticSite site;
		site.name = name->text().toStdString();
		site.g = -2. * tl2::unit<t_mat>(3);

		site.pos = tl2::create<t_vec_real>(
		{
			pos_x->GetValue(),
			pos_y->GetValue(),
			pos_z->GetValue(),
		});

		site.spin_mag = spin_mag->GetValue();
		site.spin_dir[0] = spin_x->text().toStdString();
		site.spin_dir[1] = spin_y->text().toStdString();
		site.spin_dir[2] = spin_z->text().toStdString();
		site.spin_ortho[0] = "";
		site.spin_ortho[1] = "";
		site.spin_ortho[2] = "";

		if(m_allow_ortho_spin)
		{
			if(spin_ortho_x && spin_ortho_x->text() != "" && spin_ortho_x->text() != "auto")
				site.spin_ortho[0] = spin_ortho_x->text().toStdString();
			if(spin_ortho_y && spin_ortho_y->text() != "" && spin_ortho_y->text() != "auto")
				site.spin_ortho[1] = spin_ortho_y->text().toStdString();
			if(spin_ortho_z && spin_ortho_z->text() != "" && spin_ortho_z->text() != "auto")
				site.spin_ortho[2] = spin_ortho_z->text().toStdString();
		}

		m_dyn.AddMagneticSite(std::move(site));
	}

	m_dyn.CalcMagneticSites();

	// get exchange terms
	for(int row=0; row<m_termstab->rowCount(); ++row)
	{
		auto *name = m_termstab->item(row, COL_XCH_NAME);
		auto *dist_x = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_termstab->item(row, COL_XCH_DIST_X));
		auto *dist_y = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_termstab->item(row, COL_XCH_DIST_Y));
		auto *dist_z = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_termstab->item(row, COL_XCH_DIST_Z));
		auto *interaction = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_termstab->item(row, COL_XCH_INTERACTION));
		auto *dmi_x = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_termstab->item(row, COL_XCH_DMI_X));
		auto *dmi_y = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_termstab->item(row, COL_XCH_DMI_Y));
		auto *dmi_z = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
			m_termstab->item(row, COL_XCH_DMI_Z));

		tl2::NumericTableWidgetItem<t_real>* gen_xx = nullptr;
		tl2::NumericTableWidgetItem<t_real>* gen_xy = nullptr;
		tl2::NumericTableWidgetItem<t_real>* gen_xz = nullptr;
		tl2::NumericTableWidgetItem<t_real>* gen_yx = nullptr;
		tl2::NumericTableWidgetItem<t_real>* gen_yy = nullptr;
		tl2::NumericTableWidgetItem<t_real>* gen_yz = nullptr;
		tl2::NumericTableWidgetItem<t_real>* gen_zx = nullptr;
		tl2::NumericTableWidgetItem<t_real>* gen_zy = nullptr;
		tl2::NumericTableWidgetItem<t_real>* gen_zz = nullptr;
		if(m_allow_general_J)
		{
			gen_xx = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_termstab->item(row, COL_XCH_GEN_XX));
			gen_xy = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_termstab->item(row, COL_XCH_GEN_XY));
			gen_xz = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_termstab->item(row, COL_XCH_GEN_XZ));
			gen_yx = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_termstab->item(row, COL_XCH_GEN_YX));
			gen_yy = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_termstab->item(row, COL_XCH_GEN_YY));
			gen_yz = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_termstab->item(row, COL_XCH_GEN_YZ));
			gen_zx = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_termstab->item(row, COL_XCH_GEN_ZX));
			gen_zy = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_termstab->item(row, COL_XCH_GEN_ZY));
			gen_zz = static_cast<tl2::NumericTableWidgetItem<t_real>*>(
				m_termstab->item(row, COL_XCH_GEN_ZZ));
		}

		auto atom_1_idx = GetTermAtomIndex(row, 0);
		auto atom_2_idx = GetTermAtomIndex(row, 1);

		if(!name || !atom_1_idx || !atom_2_idx ||
			!dist_x || !dist_y || !dist_z ||
			!interaction || !dmi_x || !dmi_y || !dmi_z)
		{
			std::cerr << "Invalid entry in couplings table row "
				<< row << "." << std::endl;
			continue;
		}

		t_magdyn::ExchangeTerm term;
		term.name = name->text().toStdString();
		term.site1 = *atom_1_idx;
		term.site2 = *atom_2_idx;
		term.dist = tl2::create<t_vec_real>(
		{
			dist_x->GetValue(),
			dist_y->GetValue(),
			dist_z->GetValue(),
		});

		//term.J = interaction->GetValue();
		term.J = interaction->text().toStdString();

		if(m_use_dmi->isChecked())
		{
			term.dmi[0] = dmi_x->text().toStdString();
			term.dmi[1] = dmi_y->text().toStdString();
			term.dmi[2] = dmi_z->text().toStdString();
		}

		if(m_allow_general_J && m_use_genJ->isChecked())
		{
			term.Jgen[0][0] = gen_xx->text().toStdString();
			term.Jgen[0][1] = gen_xy->text().toStdString();
			term.Jgen[0][2] = gen_xz->text().toStdString();
			term.Jgen[1][0] = gen_yx->text().toStdString();
			term.Jgen[1][1] = gen_yy->text().toStdString();
			term.Jgen[1][2] = gen_yz->text().toStdString();
			term.Jgen[2][0] = gen_zx->text().toStdString();
			term.Jgen[2][1] = gen_zy->text().toStdString();
			term.Jgen[2][2] = gen_zz->text().toStdString();
		}

		m_dyn.AddExchangeTerm(std::move(term));
	}

	m_dyn.CalcExchangeTerms();
	//m_dyn.CalcIndices();

	// ground state energy
	std::ostringstream ostrGS;
	ostrGS.precision(g_prec_gui);
	ostrGS << "E0 = " << m_dyn.CalcGroundStateEnergy() << " meV";
	m_statusFixed->setText(ostrGS.str().c_str());
}



/**
 * open the table import dialog
 */
void MagDynDlg::ShowTableImporter()
{
	if(!m_table_import_dlg)
	{
		m_table_import_dlg = new TableImportDlg(this, m_sett);

		connect(m_table_import_dlg, &TableImportDlg::SetAtomsSignal,
			this, &MagDynDlg::ImportAtoms);
		connect(m_table_import_dlg, &TableImportDlg::SetCouplingsSignal,
				this, &MagDynDlg::ImportCouplings);
	}

	m_table_import_dlg->show();
	m_table_import_dlg->raise();
	m_table_import_dlg->activateWindow();
}



/**
 * import magnetic site positions from table dialog
 */
void MagDynDlg::ImportAtoms(const std::vector<TableImportAtom>& atompos_vec)
{
	BOOST_SCOPE_EXIT(this_)
	{
		this_->m_ignoreCalc = false;
		if(this_->m_autocalc->isChecked())
			this_->CalcAll();
	} BOOST_SCOPE_EXIT_END
	m_ignoreCalc = true;

	DelTabItem(m_sitestab, -1);  // remove original sites

	for(const TableImportAtom& atompos : atompos_vec)
	{
		t_real pos_x = 0, pos_y = 0, pos_z = 0;
		std::string spin_x = "0", spin_y = "0", spin_z = "1";
		t_real spin_mag = 1;

		std::string name = "";
		if(atompos.name) name = *atompos.name;
		if(atompos.x) pos_x = *atompos.x;
		if(atompos.y) pos_y = *atompos.y;
		if(atompos.z) pos_z = *atompos.z;
		if(atompos.Sx) spin_x = tl2::var_to_str(*atompos.Sx);
		if(atompos.Sy) spin_y = tl2::var_to_str(*atompos.Sy);
		if(atompos.Sz) spin_z = tl2::var_to_str(*atompos.Sz);
		if(atompos.Smag) spin_mag = *atompos.Smag;

		if(name == "")
		{
			// default site name
			std::ostringstream ostrName;
			ostrName << "site_" << (++m_curSiteCtr);
			name = ostrName.str();
		}

		AddSiteTabItem(-1, name,
			pos_x, pos_y, pos_z,
			spin_x, spin_y, spin_z, spin_mag);
	}
}



/**
 * import magnetic couplings from table dialog
 */
void MagDynDlg::ImportCouplings(const std::vector<TableImportCoupling>& couplings)
{
	BOOST_SCOPE_EXIT(this_)
	{
		this_->m_ignoreCalc = false;
		if(this_->m_autocalc->isChecked())
			this_->CalcAll();
	} BOOST_SCOPE_EXIT_END
	m_ignoreCalc = true;

	DelTabItem(m_termstab, -1);  // remove original couplings

	for(const TableImportCoupling& coupling : couplings)
	{
		t_size atom_1 = 0, atom_2 = 0;
		t_real dist_x = 0, dist_y = 0, dist_z = 0;
		std::string J = "0";
		std::string dmi_x = "0", dmi_y = "0", dmi_z = "0";

		std::string name = "";
		if(coupling.name) name = *coupling.name;
		if(coupling.atomidx1) atom_1 = *coupling.atomidx1;
		if(coupling.atomidx2) atom_2 = *coupling.atomidx2;
		if(coupling.dx) dist_x = *coupling.dx;
		if(coupling.dy) dist_y = *coupling.dy;
		if(coupling.dz) dist_z = *coupling.dz;
		if(coupling.J) J = tl2::var_to_str(*coupling.J);
		if(coupling.dmix) dmi_x = tl2::var_to_str(*coupling.dmix);
		if(coupling.dmiy) dmi_y = tl2::var_to_str(*coupling.dmiy);
		if(coupling.dmiz) dmi_z = tl2::var_to_str(*coupling.dmiz);

		if(name == "")
		{
			// default coupling name
			std::ostringstream ostrName;
			ostrName << "coupling_" << (++m_curCouplingCtr);
			name = ostrName.str();
		}

		AddTermTabItem(-1, name, atom_1, atom_2,
			dist_x, dist_y, dist_z,
			J, dmi_x, dmi_y, dmi_z);
	}
}
