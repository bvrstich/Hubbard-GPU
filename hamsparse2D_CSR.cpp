/* Copyright (C) 2012  Ward Poelmans

This file is part of Hubbard-GPU.

Hubbard-GPU is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Hubbard-GPU is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Hubbard-GPU.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <cstdlib>
#include <cmath>
#include "hamsparse2D_CSR.h"

/**
 * Constructor of the SparseHamiltonian2D_CSR class
 * @param L The Length of the 2D grid
 * @param D The depth of the 2D grid
 * @param Nu Number of Up Electrons
 * @param Nd Number of Down Electrons
 * @param J The hopping strengh
 * @param U The onsite interaction strength
 */
SparseHamiltonian2D_CSR::SparseHamiltonian2D_CSR(int L, int D, int Nu, int Nd, double J, double U)
    : HubHam2D(L,D,Nu,Nd,J,U)
{
}

/**
 * Destructor of the SparseHamiltonian2D_CSR class
 */
SparseHamiltonian2D_CSR::~SparseHamiltonian2D_CSR()
{
}

/**
 * Builds and fills the sparse hamiltonian
 */
void SparseHamiltonian2D_CSR::BuildSparseHam()
{
    if( !baseUp.size() || !baseDown.size() )
    {
	std::cerr << "Build base before building Hamiltonian" << std::endl;
	return;
    }

    unsigned int NumUp = baseUp.size();
    unsigned int NumDown = baseDown.size();

    // by convention the last element of the row array is nnz
    Up_row.reserve(NumUp+1);
    Down_row.reserve(NumDown+1);
    Up_row.push_back(0);
    Down_row.push_back(0);
    int count = 0;

    for(unsigned int a=0;a<NumUp;a++)
    {
	for(unsigned int b=0;b<NumUp;b++)
	{
	    int result = hopping(baseUp[a], baseUp[b]);

	    if(result != 0)
	    {
                Up_data_CSR.push_back(J*result);
                Up_col.push_back(b);
                count++;
	    }
	}
        Up_row.push_back(count);
    }


    count = 0;

    for(unsigned int a=0;a<NumDown;a++)
    {
	for(unsigned int b=0;b<NumDown;b++)
	{
	    int result = hopping(baseDown[a], baseDown[b]);

	    if(result != 0)
	    {
                Down_data_CSR.push_back(J*result);
                Down_col.push_back(b);
                count++;
	    }
	}
        Down_row.push_back(count);
    }

}

/**
 * Prints the Sparse Hamiltonian
 */
void SparseHamiltonian2D_CSR::PrintSparse() const
{
    unsigned int NumUp = baseUp.size();
    unsigned int NumDown = baseDown.size();

    std::cout << "Up:" << std::endl;

    unsigned int count = 0;

    for(unsigned int i=0;i<NumUp;i++)
    {
	for(unsigned int j=0;j<NumUp;j++)
            if( count < Up_data_CSR.size() && Up_col[count] == j )
                std::cout << Up_data_CSR[count++] << "\t";
            else
		std::cout << "0\t";

	std::cout << std::endl;
    }


    std::cout << "Down:" << std::endl;
    count = 0;

    for(unsigned int i=0;i<NumDown;i++)
    {
	for(unsigned int j=0;j<NumDown;j++)
            if( count < Down_data_CSR.size() && Down_col[count] == j )
                std::cout << Down_data_CSR[count++] << "\t";
            else
		std::cout << "0\t";

	std::cout << std::endl;
    }
}

/**
 * Print the raw CSR array's
 */
void SparseHamiltonian2D_CSR::PrintRawCSR() const
{
    std::cout << "Up:" << std::endl;

    std::cout << "Data(" << Up_data_CSR.size() << "):" << std::endl;
    for(unsigned int i=0;i<Up_data_CSR.size();i++)
        std::cout << Up_data_CSR[i] << " ";
    std::cout << std::endl;

    std::cout << "Col indices:" << std::endl;
    for(unsigned int i=0;i<Up_col.size();i++)
        std::cout << Up_col[i] << " ";
    std::cout << std::endl;

    std::cout << "Row indices:" << std::endl;
    for(unsigned int i=0;i<Up_row.size();i++)
        std::cout << Up_row[i] << " ";
    std::cout << std::endl;

    std::cout << "Down:" << std::endl;

    std::cout << "Data(" << Down_data_CSR.size() << "):" << std::endl;
    for(unsigned int i=0;i<Down_data_CSR.size();i++)
        std::cout << Down_data_CSR[i] << " ";
    std::cout << std::endl;

    std::cout << "Col indices:" << std::endl;
    for(unsigned int i=0;i<Down_col.size();i++)
        std::cout << Down_col[i] << " ";
    std::cout << std::endl;

    std::cout << "Row indices:" << std::endl;
    for(unsigned int i=0;i<Down_row.size();i++)
        std::cout << Down_row[i] << " ";
    std::cout << std::endl;
}

/**
 * Matrix vector product with (sparse) hamiltonian: y = ham*x + alpha*y
 * @param x the x vector
 * @param y the y vector
 * @param alpha the scaling value
 */
void SparseHamiltonian2D_CSR::mvprod(double *x, double *y, double alpha) const
{
    int NumUp = baseUp.size();
    int NumDown = baseDown.size();

    int incx = 1;

    for(int i=0;i<NumUp;i++)
    {
#pragma omp parallel for
        for(int k=0;k<NumDown;k++)
        {
            // the interaction part
            y[i*NumDown+k] = alpha*y[i*NumDown+k] + U * CountBits(baseUp[i] & baseDown[k]) * x[i*NumDown+k];

            // the Hop_down part
            for(unsigned int l=Down_row[k];l<Down_row[k+1];l++)
                y[i*NumDown+k] += Down_data_CSR[l] * x[i*NumDown + Down_col[l]];
        }

        // the Hop_Up part
        for(unsigned int l=Up_row[i];l<Up_row[i+1];l++)
            daxpy_(&NumDown,&Up_data_CSR[l],&x[Up_col[l]*NumDown],&incx,&y[i*NumDown],&incx);
    }
}

/**
 * Builds the interaction diagonal
 * @return pointer to interaction vector. You have to free this yourself
 */
double* SparseHamiltonian2D_CSR::Umatrix() const
{
    double *Umat = new double[getDim()];

    int NumDown = baseDown.size();

    for(int i=0;i<getDim();i++)
    {
	int a = i / NumDown;
	int b = i % NumDown;
	Umat[i] = U * CountBits(baseUp[a] & baseDown[b]);
    }

    return Umat;
}

/* vim: set ts=8 sw=4 tw=0 expandtab :*/
