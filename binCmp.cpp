#include <iostream>
#include <vector>
#include <NTL/ZZ.h>
#include <NTL/BasicThreadPool.h>
#include "FHE.h"
#include "timing.h"
#include "EncryptedArray.h"
#include <NTL/lzz_pXFactoring.h>
#include <assert.h>

using namespace std;

int SLOTS = 512;
int T_BITS = 8;
int N_ROUNDS = 1;

// global variables
FHEcontext *context;
FHESecKey *secretKey;
EncryptedArray *ea;

void context_setup();
void cleanup_context();

double clock_diff(const clock_t &t1, const clock_t &t2);

Ctxt* encryptBitVal (const int bit);
vector<Ctxt*> encryptIntVal (const int val, int t_bits);

int decryptBitVal (const Ctxt *ct);
int decryptIntVal(const vector<Ctxt*> enc_bits);

/******************* batching encryption and decryption *****************************/
Ctxt* batch_encryptBitVal (const vector<long> bit);
vector<Ctxt*> batch_encryptIntVal (const vector<int> val, int t_bits);
vector<long> batch_decryptBitVal (const Ctxt *ct);
vector<int> batch_decryptIntVal(const vector<Ctxt*> enc_bits);
/*************************************************************************************/

Ctxt* compute_z (int i, int j, vector<Ctxt*>& ct_x, vector<Ctxt*>& ct_y);
Ctxt* compute_t (int i, int j, vector<Ctxt*>& ct_x, vector<Ctxt*>& ct_y);

/** Greater than or Equal
 * @return Enc(1) => x>=y,
 * @return Enc(0), otherwise 
*/
Ctxt* compute_s (int i, int j, vector<Ctxt*>& ct_x, vector<Ctxt*>& ct_y);

vector<Ctxt*> computeLBPCode(vector<Ctxt*> &pixels, vector<vector<Ctxt*>> &neighbours);

void test_LBPComputation(){

    const int t_bits = T_BITS;

    vector<int> pixeli(SLOTS);
    vector<vector<int>> vecini(8);
    vector<int> lbp_codes(SLOTS);

    for(int i=0; i<SLOTS; i++) {
        pixeli[i] = rand() % 256;
    }

    for(int i=0; i<8; i++) {
        vecini[i] = vector<int>(SLOTS);
        // generate neighbours for position i.
        for(int j=0; j<SLOTS; j++) {
            vecini[i][j] = rand() % 256;
        }
    }

    // for(int i=0; i<SLOTS; i++) {
    //     cout<<"pixel: " << pixeli[i]<<" vecini: ";
    //     for(int j=0; j<8; j++) {
    //         cout<< vecini[j][i] << " ";
    //     }
    //     cout<<endl;
    // }

    // cout<<"\ncoduri LBP\n";
    for(int i=0; i<SLOTS; i++){
        int lbp_code = 0;
        for(int j=0; j<8; j++) {
            // from LSB bit to MSB bit.
            lbp_code |= (int)(vecini[j][i] >= pixeli[i]) << j;
        }
        lbp_codes[i] = lbp_code;
        // cout<<lbp_code<<" ";
    }

    // cout<<pixeli[0] <<"\ncod lbp: ";
    // for(int i=0; i<8; i++){;
    //     cout<< vecini[i][0] << " " << pixeli[0] << " ";
    //     cout << (int)(vecini[i][0] >= pixeli[0]) << "\n";
    // }

    cout << "Encrypting pixels ...\n";
    vector<Ctxt *> enc_pixels = batch_encryptIntVal(pixeli, t_bits);
    cout << "Encrypting neighbours ...\n";
    vector<vector<Ctxt*>> enc_vecini(8);
    for(int i=0; i<8; i++){
        // encrypt neighbours for position i.
        enc_vecini[i] = batch_encryptIntVal(vecini[i], t_bits);
    }
    cout << "Encryption done.\n";

    cout << "Computing LBP code ...\n";
    // vector<Ctxt*> enc_lbp_codes = computeLBPCode(enc_pixels, enc_vecini);
    vector<Ctxt*> enc_lbp_codes(8);

    clock_t t1 = clock();
    // for each neighbour compute a bit of the final LBP code.
    for(int i=0; i<8; i++) {
        // compute the result of the operation neighbour >= pixel
        // using their binary representation, pixel = [0, 255].
        enc_lbp_codes[i] = compute_s(0, t_bits-1, enc_vecini[i], enc_pixels);
    }
    clock_t t2 = clock();
    cout << "LBP code computation done.\n";

    cout << "TIMP=" << clock_diff(t1, t2) << endl;

    cout << "Decrypting LBP code ...\n";
    vector<int> dec_lbp_codes = batch_decryptIntVal(enc_lbp_codes);
    cout << "Done decryption of lbp codes\n";

    // cout << "\nhomomorphic lbp = " << dec_lbp_codes[0] << endl;
    // cout << "in the clear = " << lbp_codes[0] << endl;

    // cout << "cod LBP homomorphic=";
    // for(int i=0; i<8; i++){
    //     vector<long> dbit = batch_decryptBitVal(enc_lbp_codes[i]);
    //     for(int j=0; j<SLOTS; j++) {
    //         cout << dbit[j] << " ";
    //     }
    // }

    bool succes = true;

    cout << "Coduri LBP homomorfice: ";
    for(int i=0; i<SLOTS; i++){
        // if(lbp_codes[i] != dec_lbp_codes[i]) {
        //     cout << "ERROR at bit " << i <<"\n";
        //     cout << lbp_codes[i] << " " << dec_lbp_codes[i] << endl;
        //     succes = false; 
        //     break;
        // }
        cout<< "(" << lbp_codes[i] << ", " << dec_lbp_codes[i] << ") ";
    }
    cout<<endl;
    // if( succes == true){
    //     cout<<"SUCCES\n";
    // }

    cout<< "Cleaning up ...\n";
    // cleanup.;
    for(int i=0; i<8; i++){
        delete enc_pixels[i];
        for(int j=0; j<8; j++){
            delete enc_vecini[i][j];
        }
    }

    for(int i=0; i<enc_lbp_codes.size(); i++){
        delete enc_lbp_codes[i];
    }

    cout<<"Cleaning up done.\n";
}

void test_batch_HE(){
    int nslots = ea->size();
    int t_bits = 8;
    vector<int> vals(nslots);
    for(int i=0; i<nslots; i++) {
        vals[i] = rand() % 256;
    }

    vector<Ctxt*> vec_ctxt = batch_encryptIntVal(vals, t_bits);

    vector<int> cmp = batch_decryptIntVal(vec_ctxt);

    bool succes = true;
    for(int i=0; i<cmp.size(); i++){
        if(cmp[i] != vals[i]){
            cout<<"ERROR\n"; 
            succes = false;
            break;
        }
    }

    if(succes == true) {
        cout<<"SUCCES\n";
    }

    for(int i=0; i<vec_ctxt.size(); i++) {
        delete vec_ctxt[i];
    }
}

void test_comparison(int t){

    // for(int i=0; i<20; i++) {

    int X = rand() % 256;
    int Y = rand() % 256;
    // int X = 1630, Y = 911;
	//int X = 91, Y = 80;
	// vector<int> values;
	// values.push_back(Y);
	// values.push_back(X);
		
	vector<Ctxt*> enc_X = encryptIntVal (X, t);
    vector<Ctxt*> enc_Y = encryptIntVal (Y, t);
	
	// Ctxt *cx = vvct[0][0];
	// Ctxt *cy = vvct[1][0];
	
	// t1 = clock();
	// *cx *= *cy;
	// t2 = clock();	
	// cout << endl << "Mul time:" << clock_diff(t1, t2) << endl;	
    clock_t t1, t2;

 	// cout << endl << "Evaluating (" << X << " = " << Y << ")...."; cout.flush();
 	t1 = clock();
 	Ctxt* ct_z = compute_z (0, t-1, enc_X, enc_Y);
 	t2 = clock();
 	int dec_z = decryptBitVal(ct_z);
 	// cout << dec_z << " (" << clock_diff(t1,t2) << "ms)";
    if( dec_z != (int)(X == Y)){
        cout<<"Esec = : "<< X << " " << Y << endl;
    }
		
 	// cout << endl << "Evaluating (" << X << " > " << Y <<")...."; cout.flush();
 	t1 = clock();
 	Ctxt* ct_t = compute_t (0, t-1,  enc_X, enc_Y);
 	t2 = clock();
 	int dec_t = decryptBitVal(ct_t);
 	// cout << dec_t << " (" << clock_diff(t1,t2)<< "ms)";
     if( dec_t != (int)(X>Y)){
        cout<<"Esec > : "<< X << " " << Y << endl;
    }

	// cout << endl << "Evaluating (" << X << " >= " << Y << ")..."; cout.flush();	
	t1 = clock();
	Ctxt* ct_s = compute_s (0, t-1,  enc_X, enc_Y);
	t2 = clock();
	int dec_s = decryptBitVal(ct_s);
	// cout << dec_s << " (" << clock_diff(t1,t2) << "ms)";
    if( dec_s != (int)(X>=Y)){
        cout<<"Esec >= : "<< X << " " << Y << endl;
    }

    // cleanup.
    for(int i=0; i<t; i++){
        delete enc_X[i];
        delete enc_Y[i];
    }

    // }
}

void test_EncDec(int t_bits){
    vector<Ctxt*> enc_bits;

    for(int i=0; i<10; i++){
        int val = rand() % 256;
        // cout << val<< " ";
        enc_bits = encryptIntVal(val, t_bits);

        val == decryptIntVal(enc_bits) ? cout<<"OK\n" :  cout<<" ERROR\n";

        for(int j=0; j<enc_bits.size(); j++){
            delete enc_bits[j];
        }
    }
}

int main(int argc, char **argv)
{
    if(argc == 4) {
        SLOTS = atoi(argv[1]);
        T_BITS = atoi(argv[2]);
        N_ROUNDS = atoi(argv[3]);
    }

    context_setup(); 

    for(int i=0; i<N_ROUNDS; i++) {
        test_LBPComputation();
    }

    cleanup_context();
    cout<<"Program terminat.\n";
    return 0;
}

double clock_diff(const clock_t &t1, const clock_t &t2){
    return double(t2 - t1) / CLOCKS_PER_SEC;
}

/* A general test program that uses a mix of operations over four ciphertexts.
 * Usage: Test_General_x [ name=value ]...
 *   p       plaintext base  [ default=2 ]
 *   r       lifting  [ default=1 ]
 *   d       degree of the field extension  [ default=1 ]
 *              d == 0 => factors[0] defines extension
 *   c       number of columns in the key-switching matrices  [ default=2 ]
 *   k       security parameter  [ default=80 ]
 *   L       # of levels in the modulus chain  [ default=heuristic ]
 *   s       minimum number of slots  [ default=0 ]
 *   repeat  number of times to repeat the test  [ default=1 ]
 *   m       use specified value as modulus
 *   mvec    use product of the integers as  modulus
 *              e.g., mvec='[5 3 187]' (this overwrite the m argument)
 *   gens    use specified vector of generators
 *              e.g., gens='[562 1871 751]'
 *   ords    use specified vector of orders
 *              e.g., ords='[4 2 -4]', negative means 'bad'
 */
void context_setup(){
    long p=2;
    long r=1;
    long d=1; // 0 dupa Togan et.al;
    long c=2;
    long k=80;
    long L=30; // 16
    long s=SLOTS; 
    long w = 64; // Hamming weight of secret key

    long m = FindM(k, L, c, p, d, s, 0, /*!noPrint*/true);

    context = new FHEcontext(m, p, r);
    buildModChain(*context, L, c);

    secretKey = new FHESecKey(*context);

    const FHEPubKey& publicKey = *secretKey;
    secretKey->GenSecKey(w); 
    addSome1DMatrices(*secretKey);

    ZZX G;
    if (d == 0)
        G = context->alMod.getFactorsOverZZ()[0];
    else
        G = makeIrredPoly(p, d); 

    ea = new EncryptedArray(*context, G);
}

/*************************************************************************************/
void cleanup_context(){
    delete context;
    delete secretKey;
    delete ea;
}

/*************************************************************************************/
Ctxt* encryptBitVal (const int bit){
    long nslots = ea->size();
    // cout<<"Numar de sloturi="<<nslots<<endl;

    vector<long> xs(nslots);
    xs[0] = bit % 2;
    for(int i=1; i<nslots; i++){
        xs[i] = rand()%2;
        // cout<<xs[i]<<" ";
    }
    // cout<<endl;

    const FHEPubKey& publicKey = *secretKey;
    Ctxt *ctxt_x = new Ctxt(publicKey);
    ea->encrypt(*ctxt_x, publicKey, xs);

    return ctxt_x;
}

/*************************************************************************************/
int decryptBitVal (const Ctxt *ct){
    vector<long> decrypted;
    ea->decrypt(*ct, *secretKey, decrypted);
    // for(int i=0;i<decrypted.size(); i++){
    //     cout<<decrypted[i]<<" ";
    // }
    return decrypted[0];
}

/*************************************************************************************/
vector<Ctxt*> encryptIntVal (const int val, int t_bits){
    vector<Ctxt*> enc_val(t_bits);

    for(int i=0; i<t_bits; i++) {
        enc_val[i] = encryptBitVal( (val>>i) & 1);
        // cout<< ((val >> i) & 1) << " ";
    }
    // cout<<endl;

    return enc_val;
}

/*************************************************************************************/
int decryptIntVal(const vector<Ctxt*> enc_bits) {
    int val = 0;

    for(int i=0; i<enc_bits.size(); i++){
        val |= decryptBitVal(enc_bits[i]) << i;
        // cout<< decryptBitVal(enc_bits[i]) << " ";
    }
    // cout<<endl;

    return val;
}

/*************************************************************************************/
Ctxt* compute_z (int i, int j, vector<Ctxt*>& ct_x, vector<Ctxt*>& ct_y)
{
	Ctxt *ret = NULL;
	if (j == 1)
	{
		ret = encryptBitVal(1);
		*ret += *ct_x[i]; 
		*ret += *ct_y[i];
		return ret;
	}
	
	int l;
	l = (j%2 == 0) ? j/2: j/2 + 1; 
	//cout << endl << "compute_z...." << "j="<<j<< "; l=" << l; 
		
	ret = compute_z(i+l, j-l, ct_x, ct_y);
	Ctxt *ct = compute_z (i, l, ct_x, ct_y);	
	*ret *= *ct;
	delete ct;
	
	return ret;
}

/*************************************************************************************/
Ctxt* compute_t (int i, int j, vector<Ctxt*>& ct_x, vector<Ctxt*>& ct_y)
{
	Ctxt *ret = NULL;
	if (j == 1)
	{
		ret  = new Ctxt (*ct_x[i]);
		*ret *= *ct_y[i]; 
		*ret += *ct_x[i];
		return ret;
	}
			
	int l;
	l = (j%2 == 0) ? j/2: j/2 + 1; 

	ret = compute_t(i+l, j-l, ct_x, ct_y);
	Ctxt *ct_z = compute_z (i+l, j-l, ct_x, ct_y);
	Ctxt *ct_t = compute_t (i, l, ct_x, ct_y);
	
	*ct_z *= *ct_t;		
	*ret += *ct_z;	
	
	delete ct_z;
	delete ct_t;	
	return ret;	
}

/*************************************************************************************/
Ctxt* compute_s (int i, int j, vector<Ctxt*>& ct_x, vector<Ctxt*>& ct_y)
{
	Ctxt *ret = NULL;
	if (j == 1)
	{
		Ctxt *ct_1 = encryptBitVal(1);
		ret  = new Ctxt (*ct_x[i]);
		*ret *= *ct_y[i]; 
		*ret += *ct_y[i];
		*ret += *ct_1;
		
		delete ct_1;		
		return ret;
	}
			
	int l;
	l = (j%2 == 0) ? j/2: j/2 + 1; 

	ret = compute_t(i+l, j-l, ct_x, ct_y);
	Ctxt *ct_z = compute_z (i+l, j-l, ct_x, ct_y);
	Ctxt *ct_s = compute_s (i, l, ct_x, ct_y);
	
	*ct_z *= *ct_s;	
	*ret += *ct_z;	
	
	delete ct_z;
	delete ct_s;	
	return ret;	
}

/*************************************************************************************/
vector<Ctxt*> computeLBPCode(vector<Ctxt*> &pixels, vector<vector<Ctxt*>> &neighbours){
    assert(neighbours.size() == 8);

    vector<Ctxt*> lbpcodes(8);

    // for each neighbour compute a bit of the final LBP code.
    for(int i=0; i<8; i++) {
        // compute the result of the operation neighbour >= pixel
        // using their binary representation, pixel = [0, 255].
        lbpcodes[i] = compute_s(0, 7, pixels, neighbours[i]);
    }

    return lbpcodes;
}

/*************************************************************************************/
Ctxt* batch_encryptBitVal (const vector<long> bit){
    // long nslots = ea->size();
    // cout<<"Numar de sloturi="<<nslots<<endl;

    const FHEPubKey& publicKey = *secretKey;
    Ctxt *ctxt_x = new Ctxt(publicKey);
    ea->encrypt(*ctxt_x, publicKey, bit);

    return ctxt_x;
}

/*************************************************************************************/
vector<long> batch_decryptBitVal (const Ctxt *ct){
    vector<long> decrypted;
    ea->decrypt(*ct, *secretKey, decrypted);
    return decrypted;
}

/*************************************************************************************/
vector<Ctxt*> batch_encryptIntVal (const vector<int> val, int t_bits){
    vector<Ctxt*> enc_val(t_bits);

    for(int i=0; i<t_bits; i++) {
        // encrypt bit i for each value in the vector val.
        vector<long> bit_i(ea->size());
        for(int j=0; j<val.size(); j++){
            bit_i[j] = (val[j] >> i) & 1;
        }
        enc_val[i] = batch_encryptBitVal(bit_i);
    }

    return enc_val;
}

/*************************************************************************************/
vector<int> batch_decryptIntVal(const vector<Ctxt*> enc_bits) {
    vector<int> val(ea->size(), 0);

    for(int i=0; i<enc_bits.size(); i++){
        vector<long> tmp = batch_decryptBitVal(enc_bits[i]);

        // UPDATE bit i corresponding to all ea->size() values.
        for(int j=0; j<tmp.size(); j++){
            val[j] |= tmp[j] << i; 
        }
    }

    return val;
}