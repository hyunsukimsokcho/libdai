/*  Copyright (C) 2006-2008  Joris Mooij  [j dot mooij at science dot ru dot nl]
    Radboud University Nijmegen, The Netherlands
    
    This file is part of libDAI.

    libDAI is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    libDAI is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libDAI; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include <iostream>
#include <dai/matlab/matlab.h>


namespace dai {


using namespace std;


/* Convert vector<Factor> structure to a cell vector of CPTAB-like structs */
mxArray *Factors2mx(const vector<Factor> &Ps) {
    size_t nr = Ps.size();

    mxArray *psi = mxCreateCellMatrix(nr,1);
    
    const char *fieldnames[2];
    fieldnames[0] = "Member";
    fieldnames[1] = "P";
    
    size_t I_ind = 0;
    for( vector<Factor>::const_iterator I = Ps.begin(); I != Ps.end(); I++, I_ind++ ) {
        mxArray *Bi = mxCreateStructMatrix(1,1,2,fieldnames);

        mxArray *BiMember = mxCreateDoubleMatrix(1,I->vars().size(),mxREAL);
        double *BiMember_data = mxGetPr(BiMember);
        size_t i = 0;
        vector<mwSize> dims;
        for( VarSet::const_iterator j = I->vars().begin(); j != I->vars().end(); j++,i++ ) {
            BiMember_data[i] = j->label();
            dims.push_back( j->states() );
        }

        mxArray *BiP = mxCreateNumericArray(I->vars().size(), &(*(dims.begin())), mxDOUBLE_CLASS, mxREAL);
        double *BiP_data = mxGetPr(BiP);
        for( size_t j = 0; j < I->states(); j++ )
            BiP_data[j] = (*I)[j];

        mxSetField(Bi,0,"Member",BiMember);
        mxSetField(Bi,0,"P",BiP);
        
        mxSetCell(psi, I_ind, Bi);
    }
    return( psi );
}


/* Convert cell vector of CPTAB-like structs to vector<Factor> */
vector<Factor> mx2Factors(const mxArray *psi, long verbose) {
    set<Var> vars;
    vector<Factor> factors;

    int n1 = mxGetM(psi);
    int n2 = mxGetN(psi);
    if( n2 != 1 && n1 != 1 ) 
        mexErrMsgTxt("psi should be a Nx1 or 1xN cell matrix."); 
    size_t nr_f = n1;
    if( n1 == 1 )
        nr_f = n2;

    // interpret psi, linear cell array of cptabs
    for( size_t cellind = 0; cellind < nr_f; cellind++ ) {
        if( verbose >= 3 )
            cout << "reading factor " << cellind << ": " << endl;
        mxArray *cell = mxGetCell(psi, cellind);
        mxArray *mx_member = mxGetField(cell, 0, "Member"); 
        size_t nr_mem = mxGetN(mx_member);
        double *members = mxGetPr(mx_member);
        const mwSize *dims = mxGetDimensions(mxGetField(cell,0,"P"));
        double *factordata = mxGetPr(mxGetField(cell, 0, "P"));

        // add variables
        VarSet factorvars;
        vector<long> labels(nr_mem,0);
        if( verbose >= 3 )
            cout << "  vars: ";
        for( size_t mi = 0; mi < nr_mem; mi++ ) {
            labels[mi] = (long)members[mi];
            if( verbose >= 3 )
                cout << labels[mi] << "(" << dims[mi] << ") ";
            vars.insert( Var(labels[mi], dims[mi]) );
            factorvars |= Var(labels[mi], dims[mi]);
        }
        factors.push_back(Factor(factorvars));

        // calculate permutation matrix
        vector<size_t> perm(nr_mem,0);
        VarSet::iterator j = factorvars.begin();
        for( size_t mi = 0; mi < nr_mem; mi++,j++ ) {
            long gezocht = j->label();
            vector<long>::iterator piet = find(labels.begin(),labels.end(),gezocht);
            perm[mi] = piet - labels.begin();
        }

        if( verbose >= 3 ) {
            cout << endl << "  perm: ";
            for( vector<size_t>::iterator r=perm.begin(); r!=perm.end(); r++ )
                cout << *r << " ";
            cout << endl;
        }

        // read Factor
        vector<size_t> di(nr_mem,0);
        size_t prod = 1;
        for( size_t k = 0; k < nr_mem; k++ ) {
            di[k] = dims[k];
            prod *= dims[k];
        }
        Permute permindex( di, perm );
        for( size_t li = 0; li < prod; li++ )
            factors.back()[permindex.convert_linear_index(li)] = factordata[li];
    }

    if( verbose >= 3 ) {
        for(vector<Factor>::const_iterator I=factors.begin(); I!=factors.end(); I++ )
            cout << *I << endl;
    }

    return( factors );
}


/* Convert CPTAB-like struct to Factor */
Factor mx2Factor(const mxArray *psi) {
    mxArray *mx_member = mxGetField(psi, 0, "Member");  
    size_t nr_mem = mxGetN(mx_member);
    double *members = mxGetPr(mx_member);
    const mwSize *dims = mxGetDimensions(mxGetField(psi,0,"P"));
    double *factordata = mxGetPr(mxGetField(psi, 0, "P"));

    // add variables
    VarSet vars;
    vector<long> labels(nr_mem,0);
    for( size_t mi = 0; mi < nr_mem; mi++ ) {
        labels[mi] = (long)members[mi];
        vars |= Var(labels[mi], dims[mi]);
    }
    Factor factor(vars);

    // calculate permutation matrix
    vector<size_t> perm(nr_mem,0);
    VarSet::iterator j = vars.begin();
    for( size_t mi = 0; mi < nr_mem; mi++,j++ ) {
        long gezocht = j->label();
        vector<long>::iterator piet = find(labels.begin(),labels.end(),gezocht);
        perm[mi] = piet - labels.begin();
    }

    // read Factor
    vector<size_t> di(nr_mem,0);
    size_t prod = 1;
    for( size_t k = 0; k < nr_mem; k++ ) {
        di[k] = dims[k];
        prod *= dims[k];
    }
    Permute permindex( di, perm );
    for( size_t li = 0; li < prod; li++ )
        factor[permindex.convert_linear_index(li)] = factordata[li];

    return( factor );
}


}
