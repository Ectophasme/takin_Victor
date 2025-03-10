/**
 * 3D Ellipsoid Dialog
 * @author Tobias Weber <tobias.weber@tum.de>
 * @date 2013 - 2024
 * @license GPLv2
 *
 * ----------------------------------------------------------------------------
 * Takin (inelastic neutron scattering software package)
 * Copyright (C) 2017-2024  Tobias WEBER (Institut Laue-Langevin (ILL),
 *                          Grenoble, France).
 * Copyright (C) 2013-2017  Tobias WEBER (Technische Universitaet Muenchen
 *                          (TUM), Garching, Germany).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * ----------------------------------------------------------------------------
 */

#include "EllipseDlg3D.h"

#include <QGridLayout>
#include <QPushButton>


EllipseDlg3D::EllipseDlg3D(QWidget* pParent, QSettings* pSett)
	: QDialog(pParent, Qt::Tool), m_pSettings(pSett)
{
	setWindowTitle("Resolution Ellipsoids (HWHM Contour Surface)");
	setSizeGripEnabled(true);

	if(m_pSettings)
	{
		QFont font;
		if(m_pSettings->contains("main/font_gen") && font.fromString(m_pSettings->value("main/font_gen", "").toString()))
			setFont(font);
	}

	t_real_reso dScale = 10.;
	PlotGl* pPlotLeft = new PlotGl(this, m_pSettings, dScale);
	pPlotLeft->SetEnabled(0);
	pPlotLeft->SetPrec(g_iPrecGfx);
	pPlotLeft->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	m_pPlots.push_back(pPlotLeft);

	PlotGl* pPlotRight = new PlotGl(this, m_pSettings, dScale);
	pPlotRight->SetEnabled(0);
	pPlotRight->SetPrec(g_iPrecGfx);
	pPlotRight->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	m_pPlots.push_back(pPlotRight);

	m_pComboCoord = new QComboBox(this);
	m_pComboCoord->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	m_pComboCoord->insertItem(0, "(Q perpendicular, Q parallel, Q up) System (1/A)");
	m_pComboCoord->insertItem(1, "Crystal (hkl) System (rlu)");
	m_pComboCoord->insertItem(2, "Scattering Plane System (rlu)");

	QPushButton *pOK = new QPushButton("OK", this);
	pOK->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));

	QGridLayout *pgridLayout = new QGridLayout(this);
	pgridLayout->setContentsMargins(4, 4, 4, 4);
	pgridLayout->addWidget(pPlotLeft, 0, 0, 1, 2);
	pgridLayout->addWidget(pPlotRight, 0, 2, 1, 2);
	pgridLayout->addWidget(m_pComboCoord, 1, 0, 1, 3);
	pgridLayout->addWidget(pOK, 1, 3, 1, 1);

	m_elliProj.resize(2);
	m_elliSlice.resize(2);

	connect(m_pComboCoord, static_cast<void(QComboBox::*)(int)>
		(&QComboBox::currentIndexChanged), this, &EllipseDlg3D::Calc);
	connect(pOK, &QPushButton::clicked, this, &QDialog::accept);

	if(m_pSettings && m_pSettings->contains("reso/ellipsoid3d_geo"))
		restoreGeometry(m_pSettings->value("reso/ellipsoid3d_geo").toByteArray());
	else
		resize(800, 600);

	for(PlotGl* pPlot : m_pPlots)
		pPlot->SetEnabled(1);
}


EllipseDlg3D::~EllipseDlg3D()
{
	for(PlotGl* pPlot : m_pPlots)
		delete pPlot;
	m_pPlots.clear();
}


void EllipseDlg3D::closeEvent(QCloseEvent* pEvt)
{
	QDialog::closeEvent(pEvt);
}


void EllipseDlg3D::accept()
{
	if(m_pSettings)
		m_pSettings->setValue("reso/ellipsoid3d_geo", saveGeometry());
	QDialog::accept();
}


void EllipseDlg3D::hideEvent(QHideEvent *pEvt)
{
	for(std::size_t i=0; i<m_pPlots.size(); ++i)
		m_pPlots[i]->SetEnabled(0);
	QDialog::hideEvent(pEvt);
}


void EllipseDlg3D::showEvent(QShowEvent *pEvt)
{
	QDialog::showEvent(pEvt);
	for(std::size_t i=0; i<m_pPlots.size(); ++i)
		if(m_pPlots[i])
			m_pPlots[i]->SetEnabled(1);
}


ublas::vector<t_real_reso>
EllipseDlg3D::ProjRotatedVec(const ublas::matrix<t_real_reso>& rot,
	const ublas::vector<t_real_reso>& vec)
{
	ublas::vector<t_real_reso> vecCoord(3);

	ublas::vector<t_real_reso> x = ublas::zero_vector<t_real_reso>(3);
	x[0] = 1.;
	ublas::vector<t_real_reso> y = ublas::zero_vector<t_real_reso>(3);
	y[1] = 1.;
	ublas::vector<t_real_reso> z = ublas::zero_vector<t_real_reso>(3);
	z[2] = 1.;

	const ublas::vector<t_real_reso> vecrot_x = ublas::prod(rot, x*vec[0]);
	const ublas::vector<t_real_reso> vecrot_y = ublas::prod(rot, y*vec[1]);
	const ublas::vector<t_real_reso> vecrot_z = ublas::prod(rot, z*vec[2]);

	vecCoord[0] = std::fabs(vecrot_x[0]) + std::fabs(vecrot_y[0]) + std::fabs(vecrot_z[0]);
	vecCoord[1] = std::fabs(vecrot_x[1]) + std::fabs(vecrot_y[1]) + std::fabs(vecrot_z[1]);
	vecCoord[2] = std::fabs(vecrot_x[2]) + std::fabs(vecrot_y[2]) + std::fabs(vecrot_z[2]);

	return vecCoord;
}


void EllipseDlg3D::Calc()
{
	const EllipseCoordSys coord = static_cast<EllipseCoordSys>(m_pComboCoord->currentIndex());

	const ublas::matrix<t_real_reso> *pReso = nullptr;
	const ublas::vector<t_real_reso> *pReso_v = nullptr;
	const ublas::vector<t_real_reso> *pQavg = nullptr;

	switch(coord)
	{
		case EllipseCoordSys::Q_AVG:       // Q|| Qperp system in 1/A
			pReso = &m_reso;
			pQavg = &m_Q_avg;
			pReso_v = &m_reso_v;
			break;
		case EllipseCoordSys::RLU:         // rlu system
			pReso = &m_resoHKL;
			pQavg = &m_Q_avgHKL;
			pReso_v = &m_reso_vHKL;
			break;
		case EllipseCoordSys::RLU_ORIENT:  // rlu system
			pReso = &m_resoOrient;
			pQavg = &m_Q_avgOrient;
			pReso_v = &m_reso_vOrient;
			break;
		default:
			tl::log_err("Unknown coordinate system selected."); return;
	}


	const ublas::matrix<t_real_reso>& reso = *pReso;
	const ublas::vector<t_real_reso>& reso_v = *pReso_v;
	const t_real_reso& reso_s = m_reso_s;
	const ublas::vector<t_real_reso>& _Q_avg = *pQavg;


	int iX[] = { 0, 0 };
	int iY[] = { 1, 1 };
	int iZ[] = { 3, 2 };
	int iIntOrRem[] = { 2, 3 };

	bool bCenterOn0 = true;

	ublas::vector<t_real_reso> Q_avg = _Q_avg;
	if(bCenterOn0)
		Q_avg = ublas::zero_vector<t_real_reso>(Q_avg.size());

	for(std::size_t i = 0; i < m_pPlots.size(); ++i)
	{
		// load ellipsoid configuration from settings
		if(m_pSettings)
		{
			std::string ello_name = tl::var_to_str(i + 1);

			int x = m_pSettings->value(("reso/ellipsoid3d_" + ello_name + "_x").c_str(), -2).toInt();
			if(x > -2)
				iX[i] = tl::clamp(x, -1, 3);

			int y = m_pSettings->value(("reso/ellipsoid3d_" + ello_name + "_y").c_str(), -2).toInt();
			if(y > -2)
				iY[i] = tl::clamp(y, -1, 3);

			int z = m_pSettings->value(("reso/ellipsoid3d_" + ello_name + "_z").c_str(), -2).toInt();
			if(z > -2)
				iZ[i] = tl::clamp(z, -1, 3);

			int proj_or_rem = m_pSettings->value(("reso/ellipsoid3d_" + ello_name + "_proj_or_rem").c_str(), -2).toInt();
			if(proj_or_rem > -2)
				iIntOrRem[i] = tl::clamp(proj_or_rem, -1, 3);
		}

		m_elliProj[i] = ::calc_res_ellipsoid(
			reso, reso_v, reso_s, Q_avg, iX[i], iY[i], iZ[i], iIntOrRem[i], -1);
		m_elliSlice[i] = ::calc_res_ellipsoid(
			reso, reso_v, reso_s, Q_avg, iX[i], iY[i], iZ[i], -1, iIntOrRem[i]);

		ublas::vector<t_real_reso> vecWProj(3), vecWSlice(3);
		ublas::vector<t_real_reso> vecOffsProj(3), vecOffsSlice(3);

		vecWProj[0] = m_elliProj[i].x_hwhm;
		vecWProj[1] = m_elliProj[i].y_hwhm;
		vecWProj[2] = m_elliProj[i].z_hwhm;

		vecWSlice[0] = m_elliSlice[i].x_hwhm;
		vecWSlice[1] = m_elliSlice[i].y_hwhm;
		vecWSlice[2] = m_elliSlice[i].z_hwhm;

		vecOffsProj[0] = m_elliProj[i].x_offs;
		vecOffsProj[1] = m_elliProj[i].y_offs;
		vecOffsProj[2] = m_elliProj[i].z_offs;

		vecOffsSlice[0] = m_elliSlice[i].x_offs;
		vecOffsSlice[1] = m_elliSlice[i].y_offs;
		vecOffsSlice[2] = m_elliSlice[i].z_offs;

		m_pPlots[i]->PlotEllipsoid(vecWProj, vecOffsProj, m_elliProj[i].rot, 1);
		m_pPlots[i]->PlotEllipsoid(vecWSlice, vecOffsSlice, m_elliSlice[i].rot, 0);

		m_pPlots[i]->SetObjectUseLOD(1, 0);
		m_pPlots[i]->SetObjectUseLOD(0, 0);

		m_pPlots[i]->SetMinMax(ProjRotatedVec(m_elliProj[i].rot, vecWProj), &vecOffsProj);

		const std::string& strX = ellipse_labels(iX[i], coord);
		const std::string& strY = ellipse_labels(iY[i], coord);
		const std::string& strZ = ellipse_labels(iZ[i], coord);
		m_pPlots[i]->SetLabels(strX.c_str(), strY.c_str(), strZ.c_str());
	}
}


void EllipseDlg3D::SetParams(const EllipseDlgParams& params)
{
	static const ublas::matrix<t_real_reso> mat0 = ublas::zero_matrix<t_real_reso>(4, 4);
	static const ublas::vector<t_real_reso> vec0 = ublas::zero_vector<t_real_reso>(4);

	if(params.reso) m_reso = *params.reso; else m_reso = mat0;
	if(params.reso_v) m_reso_v = *params.reso_v; else m_reso_v = vec0;
	m_reso_s = params.reso_s;
	if(params.Q_avg) m_Q_avg = *params.Q_avg; else m_Q_avg = vec0;

	if(params.resoHKL) m_resoHKL = *params.resoHKL; else m_resoHKL = mat0;
	if(params.reso_vHKL) m_reso_vHKL = *params.reso_vHKL; else m_reso_vHKL = vec0;
	if(params.Q_avgHKL) m_Q_avgHKL = *params.Q_avgHKL; else m_Q_avgHKL = vec0;

	if(params.resoOrient) m_resoOrient = *params.resoOrient; else m_resoOrient = mat0;
	if(params.reso_vOrient) m_reso_vOrient = *params.reso_vOrient; else m_reso_vOrient = vec0;
	if(params.Q_avgOrient) m_Q_avgOrient = *params.Q_avgOrient; else m_Q_avgOrient = vec0;

	m_algo = params.algo;

	Calc();
}


void EllipseDlg3D::keyPressEvent(QKeyEvent* pEvt)
{
	for(PlotGl* pPlot : m_pPlots)
		pPlot->keyPressEvent(pEvt);

	QDialog::keyPressEvent(pEvt);
}


#include "moc_EllipseDlg3D.cpp"
