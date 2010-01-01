/** \file
 *
 * \brief Parallel Concatenated Convolutional Codes (PCCCs) using puncturing
 *
 * Implements PCCCs using a turbo decoder with two SISO RSC modules.
 *
 */

#define TO_FILE

#include "itpp/itcomm.h"
#include "SISO.h"
#include "Progress_Timer.h"

using namespace itpp;
using tr::SISO;
using tr::threshold;
using std::cout;
using std::endl;
using std::string;

int main(void)
{
    //general parameters
	bmat puncturing_patern = "1 0 1; 1 1 0";//one row is multiplied with the coder output
    string map_metric="maxlogMAP";
    ivec gen = "07 05";//octal form, feedback first
    int constraint_length = 3;
    int nb_errors_lim = 3000;
    int nb_bits_lim = int(1e6);
    int perm_len = 4096;//total number of bits in a block (with tail) (multiple of 2)
    int nb_iter = 10;//number of iterations in the turbo decoder
    vec EbN0_dB = "1:0.1:2.6";
    double Ec = 1.0;//coded bit energy

    //coding rate for punctured PCCC
    register int en,n,k;
    double R = 0.0;
    for (n = 0; n < puncturing_patern.size(); n++)
    {
		R += double(puncturing_patern(n));
    }
    int puncturing_depth = puncturing_patern.rows();
    R = double(puncturing_depth)/R;

    //other parameters
    string filename = "Res/pccc_punctured_"+to_str(perm_len)+"_"+map_metric+".it";
    int nb_bits = perm_len-(constraint_length-1);//number of bits in a block (without tail)
    vec sigma2 = (0.5*Ec/R)*pow(inv_dB(EbN0_dB), -1.0);//N0/2
    double Lc;//scaling factor
    int nb_blocks;//number of blocks
    int nb_errors;
    ivec perm(perm_len);
    ivec inv_perm(perm_len);
    bvec bits(nb_bits);
    int cod_bits_len = perm_len*gen.length();
    bmat cod1_bits;//tail is added
    bvec tail;
    bvec cod2_input;
    bmat cod2_bits;
    int rec_len = int(double(perm_len)/R);
    bvec coded_bits(rec_len);
    vec rec(rec_len);
    vec dec1_intrinsic_coded(cod_bits_len);
    vec dec2_intrinsic_coded(cod_bits_len);
    vec apriori_data(perm_len);//a priori LLR for information bits
    vec extrinsic_coded(perm_len);
    vec extrinsic_data(perm_len);
    bvec rec_bits(perm_len);
    int snr_len = EbN0_dB.length();
    mat ber(nb_iter,snr_len);
    ber.zeros();

    //Recursive Systematic Convolutional Code
    Rec_Syst_Conv_Code cc;
    cc.set_generator_polynomials(gen, constraint_length);//initial state should be the zero state

    //BPSK modulator
    BPSK bpsk;

    //AWGN channel
    AWGN_Channel channel;

    //SISO modules
    SISO siso;
    siso.set_generators(gen, constraint_length);
    siso.set_map_metric(map_metric);
    siso.set_sova_win_len(5*constraint_length);//SOVA only
    //siso.set_sova_scaling_factor(1.0);//these are parameters for original SOVA
    //siso.set_sova_threshold(INFINITY);

    //BER
    BERC berc;

    //Progress timer
    tr::Progress_Timer timer;
    timer.set_max(snr_len);

    //Randomize generators
    RNG_randomize();

    //main loop
    timer.progress(0.0);
    for (en=0;en<snr_len;en++)
    {
        channel.set_noise(sigma2(en));
        Lc = -2/sigma2(en);//normalisation factor for intrinsic information (take into account the BPSK mapping)
        nb_errors = 0;
        nb_blocks = 0;
        while ((nb_errors<nb_errors_lim) && (nb_blocks*nb_bits<nb_bits_lim))//if at the last iteration the nb. of errors is inferior to lim, then process another block
        {
            //permutation
            perm = sort_index(randu(perm_len));
            //inverse permutation
            inv_perm = sort_index(perm);

            //bits generation
            bits = randb(nb_bits);

            //parallel concatenated convolutional code
            cc.encode_tail(bits, tail, cod1_bits);//tail is added here to information bits to close the trellis
            cod2_input = concat(bits, tail);
            cc.encode(cod2_input(perm), cod2_bits);
            k = 0;
            for (n=0;n<perm_len;n++)//output with puncturing
            {
            	if ( puncturing_patern(n%puncturing_depth,0) )//systematic output
            	{
            		coded_bits(k++) = cod2_input(n);
            	}
            	if ( puncturing_patern(n%puncturing_depth,1) )//first parity output
            	{
            		coded_bits(k++) = cod1_bits(n,0);
            	}
            	if ( puncturing_patern(n%puncturing_depth,2) )//second parity output
            	{
            		coded_bits(k++) = cod2_bits(n,0);
            	}
            }

            //BPSK modulation (1->-1,0->+1) + AWGN channel
            rec = channel(bpsk.modulate_bits(coded_bits));

            //input for SISO blocks (systematic output of the CC is already used by decoder1)
            k = 0;
            for (n=0;n<perm_len;n++)
            {
                dec1_intrinsic_coded(2*n) = ( puncturing_patern(n%puncturing_depth,0) )?Lc*rec(k++):0;
                dec1_intrinsic_coded(2*n+1) = ( puncturing_patern(n%puncturing_depth,1) )?Lc*rec(k++):0;
                dec2_intrinsic_coded(2*n) = 0.0;
                dec2_intrinsic_coded(2*n+1) = ( puncturing_patern(n%puncturing_depth,2) )?Lc*rec(k++):0;
            }
            //turbo decoder
            apriori_data.zeros();//a priori LLR for information bits
            for (n=0;n<nb_iter;n++)
            {
                //first decoder
                //siso.rsc(extrinsic_coded, extrinsic_data, dec1_intrinsic_coded, apriori_data, true);
            	siso.rsc(extrinsic_coded, extrinsic_data, dec1_intrinsic_coded, apriori_data);
                //interleave
                apriori_data = extrinsic_data(perm);
                //second decoder
                siso.rsc(extrinsic_coded, extrinsic_data, dec2_intrinsic_coded, apriori_data);

                //decision
                apriori_data += extrinsic_data;//a posteriori information
                rec_bits = bpsk.demodulate_bits(-apriori_data(inv_perm));//take into account the BPSK mapping
                //count errors
                berc.clear();
                berc.count(bits, rec_bits.left(nb_bits));
                ber(n,en) += berc.get_errorrate();

                //deinterleave for the next iteration
                apriori_data = extrinsic_data(inv_perm);
            }//end iterations
            nb_errors += int(berc.get_errors());//get number of errors at the last iteration
            nb_blocks++;
        }//end blocks (while loop)

        //compute BER over all tx blocks
        ber.set_col(en, ber.get_col(en)/nb_blocks);

        //show progress
        timer.progress(1+en);
    }
    timer.toc_print();

#ifdef TO_FILE
    //save results to file
    it_file ff(filename);
    ff << Name("BER") << ber;
    ff << Name("EbN0_dB") << EbN0_dB;
    ff << Name("gen") << gen;
    ff << Name("R") << R;
    ff << Name("nb_iter") << nb_iter;
    ff << Name("perm_len") << perm_len;
    ff << Name("nb_errors_lim") << nb_errors_lim;
    ff << Name("nb_bits_lim") << nb_bits_lim;
    ff.close();
#else
    //show BER
    cout << ber << endl;
#endif

    return 0;
}

