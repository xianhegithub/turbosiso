/** \file
 * 
 * \brief EXtrinsic Information Transfer (EXIT) chart class
 */

#include "EXIT.h"

namespace tr 
{

/// EXtrinsic Information Transfer (%EXIT) chart
/** Computes the A priori Mutual Information assuming a Gaussian distribution of the a priori information and
 * the Extrinsic Mutual Information between the emitted bits and their extrinsic information
 *
 * Description:
 * - the a priori mutual information is computed using relation (14)
 * - the extrinsic mutual information is computed by estimating first the conditional Probability Density Functions (PDF),
 * given the emitted bits, and then numerically integrating according to relation (19)
 *
 * Reference: 
 * Stephan ten Brink, ''Convergence behavior of iteratively decoded parallel concatenated codes,`` 
 * IEEE Transactions on Communications, oct. 2001
 */ 

double EXIT::sigma2A;//allocate memory for static member variable

double EXIT::extrinsic_mutual_info(const itpp::vec &obs, const itpp::bvec &cond, const int &N)
{
    //initialize histogram
    itpp::Histogram<double> hist(itpp::min(obs), itpp::max(obs), N);//common definition interval for both PDFs

    //conditional PDF knowing that a bit of 0 was emitted
    itpp::ivec idx = itpp::find(cond==itpp::bin(0));
    itpp::vec cond_obs = obs(idx);
    hist.reset();//start counting
    hist.update(cond_obs);
    itpp::vec left_pdf = hist.get_pdf();//the pdf is computed without taking into account the interval length (step)
    itpp::ivec left_int = itpp::find(left_pdf!=0);//integration interval for the left PDF

    //conditional PDF knowing that a bit of 1 was emitted
    idx = itpp::find(cond==itpp::bin(1));
    cond_obs = obs(idx);
    hist.reset();//restart counting
    hist.update(cond_obs);
    itpp::vec right_pdf = hist.get_pdf();
    itpp::ivec right_int = itpp::find(right_pdf!=0);//integration interval for the right PDF

    //mutual extrinsic information
    itpp::vec left_half = itpp::elem_mult(left_pdf(left_int), itpp::log2(itpp::elem_div(2.0*left_pdf(left_int), left_pdf(left_int)+right_pdf(left_int))));
    double IE = itpp::sum(left_half)-0.5*(left_half(0)+left_half(left_half.length()-1));//numerical integration without taking into account the inteval length (see conditional PDF computation)
    itpp::vec right_half = itpp::elem_mult(right_pdf(right_int), itpp::log2(itpp::elem_div(2.0*right_pdf(right_int), left_pdf(right_int)+right_pdf(right_int))));
    IE += itpp::sum(right_half)-0.5*(right_half(0)+right_half(right_half.length()-1));//numerical integration
    IE *= 0.5;

    return IE;
}

}//namespace tr

