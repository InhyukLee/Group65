// A Signature detecting Convolutional Neural Network.
// Using:
// 1 Conv layer
// 1 Pooling layer
// 2 full connected layers
// 1 softmax regression output
//
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <math.h>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sstream>


using namespace cv;
using namespace std;

// Gradient Checking
#define G_CHECKING 0

// Conv2 parameter
#define CONV_FULL 0
#define CONV_SAME 1
#define CONV_VALID 2

// Pooling methods
//#define POOL_MAX 0

#define POOL_MEAN 1
#define POOL_MAX 2
#define POOL_STOCHASTIC 2
#define ATD at<double>
#define elif else if
int NumHiddenNeurons = 200;
int NumHiddenLayers = 2;
int nclasses = 10;
int KernelSize = 13;
int KernelAmount = 8;
int PoolingDim = 4;
int batch;
int Pooling_Methed = POOL_STOCHASTIC;

//Convolutional Kernel definition
typedef struct ConvKernel{
    Mat W;
    double b;
    Mat Wgrad;
    double bgrad;
}ConvK;

//Convolutional Layer definition
typedef struct ConvLayer{
    vector<ConvK> layer;
    int kernelAmount;
}Cvl;

//Fully connected net definition
typedef struct Network{
    Mat W;
    Mat b;
    Mat Wgrad;
    Mat bgrad;
}Ntw;

//SoftMax regression layer definition
typedef struct SoftmaxRegession{
    Mat Weight;
    Mat Wgrad;
    Mat b;
    Mat bgrad;
    double cost;
}SMR;

/* 	prints the type of the input Mat object

	param: 	int type (Mat.type())
	pre:	Mat object exists
	post:	
*/

string type2str(int type) {
  string r;

  uchar depth = type & CV_MAT_DEPTH_MASK;
  uchar chans = 1 + (type >> CV_CN_SHIFT);

  switch ( depth ) {
    case CV_8U:  r = "8U"; break;
    case CV_8S:  r = "8S"; break;
    case CV_16U: r = "16U"; break;
    case CV_16S: r = "16S"; break;
    case CV_32S: r = "32S"; break;
    case CV_32F: r = "32F"; break;
    case CV_64F: r = "64F"; break;
    default:     r = "User"; break;
  }

  r += "C";
  r += (chans+'0');

  return r;
}
/* 	Concatenates a 2D Mat object vector into 1 Mat object

	param: 	vector<vector<Mat>> &vec (reference)
	pre:	vec exists
	post:	Mat object res created adn returned
	post:	vec remains the same
*/
Mat 
concatenateMat(vector<vector<Mat> > &vec){

    int subFeatures = vec[0][0].rows * vec[0][0].cols;
    int height = vec[0].size() * subFeatures;
    int width = vec.size();
    Mat res = Mat::zeros(height, width, CV_64FC1);

    for(int i=0; i<vec.size(); i++){
        for(int j=0; j<vec[i].size(); j++){
            Rect roi = Rect(i, j * subFeatures, 1, subFeatures);
            Mat subView = res(roi);
            Mat ptmat = vec[i][j].reshape(0, subFeatures);
            ptmat.copyTo(subView);
        }
    }
    return res;
}


/* 	Concatenates a Mat object vector into 1 Mat object

	param: 	vector<Mat> &vec (reference)
	pre:	vec exists
	post:	Mat object res created and returned
	post:	vec remains the same
*/
Mat 
concatenateMat(vector<Mat> &vec){

    int height = vec[0].rows;
    int width = vec[0].cols;
    Mat res = Mat::zeros(height * width, vec.size(), CV_64FC1);
    for(int i=0; i<vec.size(); i++){
        Mat img(vec[i]);
        // reshape(int cn, int rows=0), cn is num of channels.
        Mat ptmat = img.reshape(0, height * width);
        Rect roi = cv::Rect(i, 0, ptmat.cols, ptmat.rows);
        Mat subView = res(roi);
        ptmat.copyTo(subView);
    }
    return res;
}

/* 	Unconcatenates a concatenated Mat object into a 2D 
	Mat object vector

	param: 	Mat &M (reference)
	param:	vector<vector<Mat>> &vec (reference)
	pre:	vec was concatenated through concatenateMat
	post:	input array vec modified to contain unconcatenated
			M features via push_back	
*/
void
unconcatenateMat(Mat &M, vector<vector<Mat> > &vec, int vsize){

    int sqDim = M.rows / vsize;
    int Dim = sqrt(sqDim);
    for(int i=0; i<M.cols; i++){
        vector<Mat> oneColumn;
        for(int j=0; j<vsize; j++){
            Rect roi = Rect(i, j * sqDim, 1, sqDim);
            Mat temp;
            M(roi).copyTo(temp);
            Mat img = temp.reshape(0, Dim);
            oneColumn.push_back(img);
        }
        vec.push_back(oneColumn);
    }
}

/* Returns the reverse of an integer value 	

	param: 	int i
	pre:	
	post:	new int created and returned	
*/
int 
ReverseInt (int i){
    unsigned char ch1, ch2, ch3, ch4;
    ch1 = i & 255;
    ch2 = (i >> 8) & 255;
    ch3 = (i >> 16) & 255;
    ch4 = (i >> 24) & 255;
    return((int) ch1 << 24) + ((int)ch2 << 16) + ((int)ch3 << 8) + ch4;
}


/* 	Reads a ubyte format file into a Mat object vector

	param: 	string filename
	param:	vector<Mat> &vec (reference)
	pre:	filename value is a valid file location
	post:	vec modified to contain Mat objects within
			ubyte file via push_back	
*/
void 
read_Mnist(string filename, vector<Mat> &vec){
    ifstream file(filename.c_str(), ios::binary);
    if (file.is_open()){
        int magic_number = 0;
        int number_of_images = 0;
        int n_rows = 0;
        int n_cols = 0;
        file.read((char*) &magic_number, sizeof(magic_number));
        magic_number = ReverseInt(magic_number);
        file.read((char*) &number_of_images,sizeof(number_of_images));
        number_of_images = ReverseInt(number_of_images);
        file.read((char*) &n_rows, sizeof(n_rows));
        n_rows = ReverseInt(n_rows);
        file.read((char*) &n_cols, sizeof(n_cols));
        n_cols = ReverseInt(n_cols);
        for(int i = 0; i < number_of_images; ++i){
            Mat tpmat = Mat::zeros(n_rows, n_cols, CV_8UC1);
            for(int r = 0; r < n_rows; ++r){
                for(int c = 0; c < n_cols; ++c){
                    unsigned char temp = 0;
                    file.read((char*) &temp, sizeof(temp));
                    tpmat.at<uchar>(r, c) = (int) temp;
                }
            }
            vec.push_back(tpmat);
        }
    }
}


/* 	Reads a ubyte format file into a Mat object vector

	param: 	string filename
	param:	vector<Mat> &vec (reference)
	pre:	filename value is a valid file location
	post:	vec modified to contain Mat objects within
			ubyte file via push_back	
*/
void 
read_Mnist_Label(string filename, Mat &mat)
{
    ifstream file(filename.c_str(), ios::binary);
    if (file.is_open()){
        int magic_number = 0;
        int number_of_images = 0;
        int n_rows = 0;
        int n_cols = 0;
        file.read((char*) &magic_number, sizeof(magic_number));
        magic_number = ReverseInt(magic_number);
        file.read((char*) &number_of_images,sizeof(number_of_images));
        number_of_images = ReverseInt(number_of_images);
        for(int i = 0; i < number_of_images; ++i){
            unsigned char temp = 0;
            file.read((char*) &temp, sizeof(temp));
            mat.ATD(0, i) = (double)temp;
        }
    }
}

Mat 
sigmoid(Mat &M){
    Mat temp;
    exp(-M, temp);
    return 1.0 / (temp + 1.0);
}

Mat 
dsigmoid(Mat &a){
    Mat res = 1.0 - a;
    res = res.mul(a);
    return res;
}

Mat
ReLU(Mat& M){
    Mat res(M);
    for(int i=0; i<M.rows; i++){
        for(int j=0; j<M.cols; j++){
            if(M.ATD(i, j) < 0.0) res.ATD(i, j) = 0.0;
        }
    }
    return res;
}

Mat
dReLU(Mat& M){
    Mat res = Mat::zeros(M.rows, M.cols, CV_64FC1);
    for(int i=0; i<M.rows; i++){
        for(int j=0; j<M.cols; j++){
            if(M.ATD(i, j) > 0.0) res.ATD(i, j) = 1.0;
        }
    }
    return res;
}

// Mimic rot90() in Matlab/GNU Octave.
Mat 
rot90(Mat &M, int k){
    Mat res;
    if(k == 0) return M;
    elif(k == 1){
        flip(M.t(), res, 0);
    }else{
        flip(rot90(M, k - 1).t(), res, 0);
    }
    return res;
}


// A Matlab/Octave style 2-d convolution function.
// from http://blog.timmlinder.com/2011/07/opencv-equivalent-to-matlabs-conv2-function/
Mat 
conv2(Mat &img, Mat &kernel, int convtype) {
    Mat dest;
    Mat source = img;
    if(CONV_FULL == convtype) {
        source = Mat();
        int additionalRows = kernel.rows-1, additionalCols = kernel.cols-1;
        copyMakeBorder(img, source, (additionalRows+1)/2, additionalRows/2, (additionalCols+1)/2, additionalCols/2, BORDER_CONSTANT, Scalar(0));
    }
    Point anchor(kernel.cols - kernel.cols/2 - 1, kernel.rows - kernel.rows/2 - 1);
    int borderMode = BORDER_CONSTANT;
    Mat fkernal;
    flip(kernel, fkernal, -1);
    filter2D(source, dest, img.depth(), fkernal, anchor, 0, borderMode);

    if(CONV_VALID == convtype) {
        dest = dest.colRange((kernel.cols-1)/2, dest.cols - kernel.cols/2)
                   .rowRange((kernel.rows-1)/2, dest.rows - kernel.rows/2);
    }
    return dest;
}

// get KroneckerProduct 
// for upsample
// see function kron() in Matlab/Octave
Mat
kron(Mat &a, Mat &b){

    Mat res = Mat::zeros(a.rows * b.rows, a.cols * b.cols, CV_64FC1);
    for(int i=0; i<a.rows; i++){
        for(int j=0; j<a.cols; j++){
            Rect roi = Rect(j * b.cols, i * b.rows, b.cols, b.rows);
            Mat temp = res(roi);
            Mat c = b.mul(a.ATD(i, j));
            c.copyTo(temp);
        }
    }
    return res;
}

Point
findLoc(double val, Mat &M){
    Point res = Point(0, 0);
    double minDiff = 1e8;
    for(int i=0; i<M.rows; i++){
        for(int j=0; j<M.cols; j++){
            if(val >= M.ATD(i, j) && (val - M.ATD(i, j) < minDiff)){
                minDiff = val - M.ATD(i, j);
                res.x = j;
                res.y = i;
            }
        }
    }
    return res;
}

Mat
Pooling(Mat &M, int pVert, int pHori, int poolingMethod, vector<Point> &locat, bool isTest){
    int remX = M.cols % pHori;
    int remY = M.rows % pVert;
    Mat newM;
    if(remX == 0 && remY == 0) M.copyTo(newM);
    else{
        Rect roi = Rect(remX, remY, M.cols - remX, M.rows - remY);
        M(roi).copyTo(newM);
    }
    Mat res = Mat::zeros(newM.rows / pVert, newM.cols / pHori, CV_64FC1);
    for(int i=0; i<res.rows; i++){
        for(int j=0; j<res.cols; j++){
            Mat temp;
            Rect roi = Rect(j * pHori, i * pVert, pHori, pVert);
            newM(roi).copyTo(temp);
            double val;
            // for Max Pooling
            if(POOL_MAX == poolingMethod){ 
                double minVal; 
                double maxVal; 
                Point minLoc; 
                Point maxLoc;
                minMaxLoc( temp, &minVal, &maxVal, &minLoc, &maxLoc );
                val = maxVal;
                locat.push_back(Point(maxLoc.x + j * pHori, maxLoc.y + i * pVert));
            }elif(POOL_MEAN == poolingMethod){
                // Mean Pooling
                val = sum(temp)[0] / (pVert * pHori);
            }elif(POOL_STOCHASTIC == poolingMethod){
                // Stochastic Pooling
                double sumval = sum(temp)[0];
                Mat prob = temp / sumval;
                if(isTest){
                    val = sum(prob.mul(temp))[0];
                }else{
                    RNG rng;
                    double ran = rng.uniform((double)0, (double)1);
                    double minVal; 
                    double maxVal; 
                    Point minLoc; 
                    Point maxLoc;
                    minMaxLoc( prob, &minVal, &maxVal, &minLoc, &maxLoc );
                    ran *= maxVal;
                    Point loc = findLoc(ran, prob);
                    val = temp.ATD(loc.y, loc.x);
                    locat.push_back(Point(loc.x + j * pHori, loc.y + i * pVert));
                }

            }
            res.ATD(i, j) = val;
        }
    }
    return res;
}

Mat 
UnPooling(Mat &M, int pVert, int pHori, int poolingMethod, vector<Point> &locat){
    Mat res;
    if(POOL_MEAN == poolingMethod){
        Mat one = Mat::ones(pVert, pHori, CV_64FC1);
        res = kron(M, one) / (pVert * pHori);
    }elif(POOL_MAX == poolingMethod || POOL_STOCHASTIC == poolingMethod){
        res = Mat::zeros(M.rows * pVert, M.cols * pHori, CV_64FC1);
        for(int i=0; i<M.rows; i++){
            for(int j=0; j<M.cols; j++){
                res.ATD(locat[i * M.cols + j].y, locat[i * M.cols + j].x) = M.ATD(i, j);
            }
        }
    }
    return res;
}

double 
getLearningRate(Mat &data){
    cout<<"gathering learning rate"<<endl;
    // see Yann LeCun's Efficient BackProp, 5.1 A Little Theory
    int nfeatures = data.rows;
    int nsamples = data.cols;
    //covariance matrix = x * x' ./ nfeatures;
    Mat Sigma = data * data.t() / nsamples;
    cout<<"learning rate check 1...."<<endl;
    SVD uwvT = SVD(Sigma);
    cout<<"learning rate check 2...."<<endl;
    return 0.9 / uwvT.w.ATD(0, 0);
}

void
weightRandomInit(ConvK &convk, int width){

    double epsilon = 0.1;
    convk.W = Mat::ones(width, width, CV_64FC1);
    double *pData; 
    for(int i = 0; i<convk.W.rows; i++){
        pData = convk.W.ptr<double>(i);
        for(int j=0; j<convk.W.cols; j++){
            pData[j] = randu<double>();        
        }
    }
    convk.W = convk.W * (2 * epsilon) - epsilon;
    convk.b = 0;
    convk.Wgrad = Mat::zeros(width, width, CV_64FC1);
    convk.bgrad = 0;
}

void
weightRandomInit(Ntw &ntw, int inputsize, int hiddensize, int nsamples){

    double epsilon = sqrt(6) / sqrt(hiddensize + inputsize + 1);
    double *pData;
    ntw.W = Mat::ones(hiddensize, inputsize, CV_64FC1);
    for(int i=0; i<hiddensize; i++){
        pData = ntw.W.ptr<double>(i);
        for(int j=0; j<inputsize; j++){
            pData[j] = randu<double>();
        }
    }
    ntw.W = ntw.W * (2 * epsilon) - epsilon;
    ntw.b = Mat::zeros(hiddensize, 1, CV_64FC1);
    ntw.Wgrad = Mat::zeros(hiddensize, inputsize, CV_64FC1);
    ntw.bgrad = Mat::zeros(hiddensize, 1, CV_64FC1);
}

void 
weightRandomInit(SMR &smr, int nclasses, int nfeatures){
    double epsilon = 0.01;
    smr.Weight = Mat::ones(nclasses, nfeatures, CV_64FC1);
    double *pData; 
    for(int i = 0; i<smr.Weight.rows; i++){
        pData = smr.Weight.ptr<double>(i);
        for(int j=0; j<smr.Weight.cols; j++){
            pData[j] = randu<double>();        
        }
    }
    smr.Weight = smr.Weight * (2 * epsilon) - epsilon;
    smr.b = Mat::zeros(nclasses, 1, CV_64FC1);
    smr.cost = 0.0;
    smr.Wgrad = Mat::zeros(nclasses, nfeatures, CV_64FC1);
    smr.bgrad = Mat::zeros(nclasses, 1, CV_64FC1);
}


void
ConvNetInitPrarms(Cvl &cvl, vector<Ntw> &HiddenLayers, SMR &smr, int imgDim, int nsamples){

    // Init Conv layers
    for(int j=0; j<KernelAmount; j++){
        ConvK tmpConvK;
        weightRandomInit(tmpConvK, KernelSize);
        cvl.layer.push_back(tmpConvK);
    }
    cvl.kernelAmount = KernelAmount;
    // Init Hidden layers
    int outDim = imgDim - KernelSize + 1; 
    outDim = outDim / PoolingDim;
    
    int hiddenfeatures = pow(outDim, 2) * KernelAmount;
    cout<<"check 1 matmul = "<<hiddenfeatures<<endl;
    Ntw tpntw;
    weightRandomInit(tpntw, hiddenfeatures, NumHiddenNeurons, nsamples);
    HiddenLayers.push_back(tpntw);
    for(int i=1; i<NumHiddenLayers; i++){
        Ntw tpntw2;
        weightRandomInit(tpntw2, NumHiddenNeurons, NumHiddenNeurons, nsamples);
        HiddenLayers.push_back(tpntw2);
    }
    // Init Softmax layer
    weightRandomInit(smr, nclasses, NumHiddenNeurons);
}

Mat
getNetworkActivation(Ntw &ntw, Mat &data){
    Mat acti;
    acti = ntw.W * data + repeat(ntw.b, 1, data.cols);
    acti = sigmoid(acti);
    return acti;
}


void
getNetworkCost(vector<Mat> &x, Mat &y, Cvl &cvl, vector<Ntw> &hLayers, SMR &smr, double lambda){

    int nsamples = x.size();
    // Conv & Pooling
    vector<vector<Mat> > Conv1st;
    vector<vector<Mat> > Pool1st;
    vector<vector<vector<Point> > > PoolLoc;
    for(int k=0; k<nsamples; k++){
        vector<Mat> tpConv1st;
        vector<Mat> tpPool1st;
        vector<vector<Point> > PLperSample;
        for(int i=0; i<cvl.kernelAmount; i++){
            vector<Point> PLperKernel;
            Mat temp = rot90(cvl.layer[i].W, 2);
            Mat tmpconv = conv2(x[k], temp, CONV_VALID);
            tmpconv += cvl.layer[i].b;
            //tmpconv = sigmoid(tmpconv);
            tmpconv = ReLU(tmpconv);
            tpConv1st.push_back(tmpconv);
            tmpconv = Pooling(tmpconv, PoolingDim, PoolingDim, Pooling_Methed, PLperKernel, false);
            PLperSample.push_back(PLperKernel);
            tpPool1st.push_back(tmpconv);
        }
        PoolLoc.push_back(PLperSample);
        Conv1st.push_back(tpConv1st);
        Pool1st.push_back(tpPool1st);
    }
    Mat convolvedX = concatenateMat(Pool1st);

    // full connected layers
    vector<Mat> acti;
    acti.push_back(convolvedX);
    for(int i=1; i<=NumHiddenLayers; i++){
	cout<< hLayers[i-1].W.cols<<" =? "<<acti[i-1].rows<<"\n";
        Mat tmpacti = hLayers[i - 1].W * acti[i - 1] + repeat(hLayers[i - 1].b, 1, convolvedX.cols);
        acti.push_back(sigmoid(tmpacti));
    }

    Mat M = smr.Weight * acti[acti.size() - 1] + repeat(smr.b, 1, nsamples);
    Mat tmp;
    reduce(M, tmp, 0, CV_REDUCE_MAX);
    M -= repeat(tmp, M.rows, 1);
    Mat p;
    exp(M, p);
    reduce(p, tmp, 0, CV_REDUCE_SUM);
    divide(p, repeat(tmp, p.rows, 1), p);

    // softmax regression
    Mat groundTruth = Mat::zeros(nclasses, nsamples, CV_64FC1);
    for(int i=0; i<nsamples; i++){
        groundTruth.ATD(y.ATD(0, i), i) = 1.0;
    }
    Mat logP;
    log(p, logP);
    logP = logP.mul(groundTruth);
    smr.cost = - sum(logP)[0] / nsamples;
    pow(smr.Weight, 2.0, tmp);
    smr.cost += sum(tmp)[0] * lambda / 2;
    for(int i=0; i<cvl.kernelAmount; i++){
        pow(cvl.layer[i].W, 2.0, tmp);
        smr.cost += sum(tmp)[0] * lambda / 2;
    }

    // bp - softmax
    tmp = (groundTruth - p) * acti[acti.size() - 1].t();
    tmp /= -nsamples;
    smr.Wgrad = tmp + lambda * smr.Weight;
    reduce((groundTruth - p), tmp, 1, CV_REDUCE_SUM);
    smr.bgrad = tmp / -nsamples;

    // bp - full connected
    vector<Mat> delta(acti.size());
    delta[delta.size() -1] = -smr.Weight.t() * (groundTruth - p);
    delta[delta.size() -1] = delta[delta.size() -1].mul(dsigmoid(acti[acti.size() - 1]));
    for(int i = delta.size() - 2; i >= 0; i--){
        delta[i] = hLayers[i].W.t() * delta[i + 1];
        if(i > 0) delta[i] = delta[i].mul(dsigmoid(acti[i]));
    }
    for(int i=NumHiddenLayers - 1; i >=0; i--){
        hLayers[i].Wgrad = delta[i + 1] * acti[i].t();
        hLayers[i].Wgrad /= nsamples;
        reduce(delta[i + 1], tmp, 1, CV_REDUCE_SUM);
        hLayers[i].bgrad = tmp / nsamples;
    }
    //bp - Conv layer
    Mat one = Mat::ones(PoolingDim, PoolingDim, CV_64FC1);
    vector<vector<Mat> > Delta;
    vector<vector<Mat> > convDelta;
    unconcatenateMat(delta[0], Delta, cvl.kernelAmount);
    for(int k=0; k<Delta.size(); k++){
        vector<Mat> tmp;
        for(int i=0; i<Delta[k].size(); i++){
            Mat upDelta = UnPooling(Delta[k][i], PoolingDim, PoolingDim, Pooling_Methed, PoolLoc[k][i]);
            //upDelta = upDelta.mul(dsigmoid(Conv1st[k][i]));
            upDelta = upDelta.mul(dReLU(Conv1st[k][i]));
            tmp.push_back(upDelta);
        }
        convDelta.push_back(tmp); 
    }
    
    for(int j=0; j<cvl.kernelAmount; j++){
        Mat tpgradW = Mat::zeros(KernelSize, KernelSize, CV_64FC1);
        double tpgradb = 0.0;
        for(int i=0; i<nsamples; i++){
            Mat temp = rot90(convDelta[i][j], 2);
            tpgradW += conv2(x[i], temp, CONV_VALID);
            tpgradb += sum(convDelta[i][j])[0];
        }
        cvl.layer[j].Wgrad = tpgradW / nsamples + lambda * cvl.layer[j].W;
        cvl.layer[j].bgrad = tpgradb / nsamples;
    }
    // deconstruct
    for(int i=0; i<Conv1st.size(); i++){
        Conv1st[i].clear();
        Pool1st[i].clear();
    }
    Conv1st.clear();
    Pool1st.clear();
    for(int i=0; i<PoolLoc.size(); i++){
        for(int j=0; j<PoolLoc[i].size(); j++){
            PoolLoc[i][j].clear();
        }
        PoolLoc[i].clear();
    }
    PoolLoc.clear();
    acti.clear();
    delta.clear();
}

void
gradientChecking(Cvl &cvl, vector<Ntw> &hLayers, SMR &smr, vector<Mat> &x, Mat &y, double lambda){
    //Gradient Checking (remember to disable this part after you're sure the 
    //cost function and dJ function are correct)
    getNetworkCost(x, y, cvl, hLayers, smr, lambda);
    Mat grad(cvl.layer[0].Wgrad);
    cout<<"test network !!!!"<<endl;
    double epsilon = 1e-4;
    for(int i=0; i<cvl.layer[0].W.rows; i++){
        for(int j=0; j<cvl.layer[0].W.cols; j++){
            double memo = cvl.layer[0].W.ATD(i, j);
            cvl.layer[0].W.ATD(i, j) = memo + epsilon;
            getNetworkCost(x, y, cvl, hLayers, smr, lambda);
            double value1 = smr.cost;
            cvl.layer[0].W.ATD(i, j) = memo - epsilon;
            getNetworkCost(x, y, cvl, hLayers, smr, lambda);
            double value2 = smr.cost;
            double tp = (value1 - value2) / (2 * epsilon);
            cout<<i<<", "<<j<<", "<<tp<<", "<<grad.ATD(i, j)<<", "<<grad.ATD(i, j) / tp<<endl;
            cvl.layer[0].W.ATD(i, j) = memo;
        }
    }
}

void
trainNetwork(vector<Mat> &x, Mat &y, Cvl &cvl, vector<Ntw> &HiddenLayers, SMR &smr, double lambda, int MaxIter, double lrate){
	
    if (G_CHECKING){
	cout<<"gradient checking....."<<endl;
        gradientChecking(cvl, HiddenLayers, smr, x, y, lambda);
    }else{
	
        int converge = 0;
        double lastcost = 0.0;
        //double lrate = getLearningRate(x);
        cout<<"Network Learning, trained learning rate: "<<lrate<<endl;
        while(converge < MaxIter){

            int randomNum = ((long)rand() + (long)rand()) % (x.size() - batch);
            vector<Mat> batchX;
            for(int i=0; i<batch; i++){
                batchX.push_back(x[i + randomNum]);
            }
            Rect roi = Rect(randomNum, 0, batch, y.rows);
            Mat batchY = y(roi);

            getNetworkCost(batchX, batchY, cvl, HiddenLayers, smr, lambda);

            cout<<"learning step: "<<converge<<", Cost function value = "<<smr.cost<<", randomNum = "<<randomNum<<endl;
            if(fabs((smr.cost - lastcost) / smr.cost) <= 1e-7 && converge > 0) break;
            if(smr.cost <= 0) break;
            lastcost = smr.cost;
            smr.Weight -= lrate * smr.Wgrad;
            smr.b -= lrate * smr.bgrad;
            for(int i=0; i<HiddenLayers.size(); i++){
                HiddenLayers[i].W -= lrate * HiddenLayers[i].Wgrad;
                HiddenLayers[i].b -= lrate * HiddenLayers[i].bgrad;
            }
            for(int i=0; i<cvl.kernelAmount; i++){
                cvl.layer[i].W -= lrate * cvl.layer[i].Wgrad;
                cvl.layer[i].b -= lrate * cvl.layer[i].bgrad;
            }
            ++ converge;
        }
        
    }
}

void
readData(vector<Mat> &x, Mat &y, string xpath, string ypath, int number_of_images){

    //read MNIST image into OpenCV Mat vector
    read_Mnist(xpath, x);
    for(int i=0; i<x.size(); i++){
        x[i].convertTo(x[i], CV_64FC1, 1.0/255, 0);
    }
    //read MNIST label into double vector
    y = Mat::zeros(1, number_of_images, CV_64FC1);
    read_Mnist_Label(ypath, y);
}

Mat 
resultProdict(vector<Mat> &x, Cvl &cvl, vector<Ntw> &hLayers, SMR &smr, double lambda){
    cout<<"result prodict process started..."<<endl;
    int nsamples = x.size();
    vector<vector<Mat> > Conv1st;
    vector<vector<Mat> > Pool1st;
    vector<Point> PLperKernel;
    for(int k=0; k<nsamples; k++){
        vector<Mat> tpConv1st;
        vector<Mat> tpPool1st;
        for(int i=0; i<cvl.kernelAmount; i++){
            Mat temp = rot90(cvl.layer[i].W, 2);
	    string ty = type2str(x[k].type());
	    cout<<"index "<<k+1<<", type = "<<ty<<" out of "<<nsamples<<endl;
            Mat tmpconv = conv2(x[k], temp, CONV_VALID);
            tmpconv += cvl.layer[i].b;
            //tmpconv = sigmoid(tmpconv);
            tmpconv = ReLU(tmpconv);
            tpConv1st.push_back(tmpconv);
            tmpconv = Pooling(tmpconv, PoolingDim, PoolingDim, Pooling_Methed, PLperKernel, true);
            tpPool1st.push_back(tmpconv);
	    //if(1 % 20 == 0)
		//cout<<"[-]";
        }
        Conv1st.push_back(tpConv1st);
        Pool1st.push_back(tpPool1st);
    }
    cout<<"conv layer loaded...\npooling layer loaded...\n";
    Mat convolvedX = concatenateMat(Pool1st);

    vector<Mat> acti;
    acti.push_back(convolvedX);
    for(int i=1; i<=NumHiddenLayers; i++){
        cout<< hLayers[i-1].W.cols <<" =? "<<acti[i-1].rows<<"\n";
	Mat tmpacti = hLayers[i - 1].W * acti[i - 1] + repeat(hLayers[i - 1].b, 1, convolvedX.cols);
        acti.push_back(sigmoid(tmpacti));
	//cout<<"[-]";
    }
    cout<<"loaded hidden layers...\n";


    Mat M = smr.Weight * acti[acti.size() - 1] + repeat(smr.b, 1, nsamples);
    Mat tmp;
    reduce(M, tmp, 0, CV_REDUCE_MAX);
    M -= repeat(tmp, M.rows, 1);
    Mat p;
    exp(M, p);
    reduce(p, tmp, 0, CV_REDUCE_SUM);
    divide(p, repeat(tmp, p.rows, 1), p);
    log(p, tmp);

    Mat result = Mat::ones(1, tmp.cols, CV_64FC1);
    for(int i=0; i<tmp.cols; i++){
        double maxele = tmp.ATD(0, i);
        int which = 0;
        for(int j=1; j<tmp.rows; j++){
            if(tmp.ATD(j, i) > maxele){
                maxele = tmp.ATD(j, i);
                which = j;
            }
	//cout<<"[-]";
        }
        result.ATD(0, i) = which;
    }
    cout<<"results gathered...\n";

    // deconstruct
    for(int i=0; i<Conv1st.size(); i++){
        Conv1st[i].clear();
        Pool1st[i].clear();
    }
    Conv1st.clear();
    Pool1st.clear();
    acti.clear();
    cout<<"forward pass finished, memory cleared...\n";
    return result;
}

void
saveWeight(Mat &M, string s){
    s += ".txt";
    FILE *pOut = fopen(s.c_str(), "w+");
    for(int i=0; i<M.rows; i++){
        for(int j=0; j<M.cols; j++){
            fprintf(pOut, "%lf", M.ATD(i, j));
            if(j == M.cols - 1) fprintf(pOut, "\n");
            else fprintf(pOut, " ");

        }
    }
    fclose(pOut);
}
void
loadWeight(Mat &M, string s){
	s += ".txt";  
	cout<<"loading file : "<<s<<endl;
	FILE *pIn = fopen(s.c_str(), "r");
	double n;
	int i=0;
	int j=0;
	int count=0;
	if(pIn == NULL){
		cout<<"file could not be read"<<endl;
		return;
	}
	cout<<"num cols = "<<M.cols<<" ; \n";
	cout<<"num rows = "<<M.rows<<" ; \n";
	while(fscanf(pIn,"%lf",&n) > 0 ){
		//cout<<"%";
		if(j == M.cols){
			i++;
			j=0;
		}
		if(i == M.rows)
			break;
		//cout<<"&";
		M.ATD(i,j) = (double) n;
		//cout<<"#";
		//cout<<"loaded weight "<< M.ATD(i,j) <<" @index "<<i<<", "<<j<<" in file "<<s<<"\n";
		count++;
		j++;
	}
	cout<<"\nload sucess from loadWeight()....\n";
	cout<<"elements loaded = "<<count<<endl;
	fclose(pIn);

}



void
saveNetwork(Cvl &cv1, vector<Ntw> &HiddenLayers, SMR &smr, int imgDim, int nsamples){
	FILE *pOut1 = fopen("cv1dubs.txt","w+");
	FILE *pOut2 = fopen("SMRcost.txt","w+.");
	FILE *pOut3 = fopen("cv1KernNum.txt","w+");
	FILE *pOut4 = fopen("imgDim-Nsamples.txt","w+");
	
	string saver ="";
	//save numsamples and imgdim
	fprintf(pOut4,"%i\n",imgDim);
	fprintf(pOut4,"%i\n",nsamples);
	stringstream out;
	//save weights in Conv layers
	for(int i=0; i < cv1.layer.size(); i++){
		saver = "wCV";
		out << i;
		saver += out.str();
		saveWeight(cv1.layer[i].W,saver);
		saver = "wgradCV";
		saver += out.str();
		saveWeight(cv1.layer[i].Wgrad,saver);
		fprintf(pOut1,"%lf\n",cv1.layer[i].b);
		fprintf(pOut1,"%lf\n",cv1.layer[i].bgrad);
		cout<<"saved conv layer "<<i<<endl;
		out.str(string());	
	}
	//save kernel amount
	fprintf(pOut3,"%i",cv1.kernelAmount);
	
	//save structure of hidden layer network
	for(int i=0; i < HiddenLayers.size(); i++){
		out << i;
		string index = out.str();
		
		saver = "wn";
		saver += index;
		saveWeight(HiddenLayers[i].W,saver);
		
		saver = "b";
		saver += index;
		saveWeight(HiddenLayers[i].b,saver);
		
		saver = "HWgrad";
		saver += index;
		saveWeight(HiddenLayers[i].Wgrad,saver);
		
		saver = "Hbgrad";
		saver += index;
		saveWeight(HiddenLayers[i].bgrad,saver);
		cout <<"saved hidden layer "<<i<<endl;
		out.str(string());
		
	}
	//save soft max regression layer
		
	saver = "wS1";
	saveWeight(smr.Weight,saver);
	
	saver = "bS1";
	saveWeight(smr.b,saver);
	
	saver = "HWgradS1";
	saveWeight(smr.Wgrad,saver);
		
	saver = "HbgradS1";
	saveWeight(smr.bgrad,saver);
	fprintf(pOut2,"%lf",smr.cost);
	
}

void
loadNetwork(Cvl &cv1, vector<Ntw> &HiddenLayers, SMR &smr, int &imgDim, int &nsamples){
	FILE *pOut1 = fopen("cv1dubs.txt","r");
	FILE *pOut2 = fopen("SMRcost.txt","r");
	FILE *pOut4 = fopen("imgDim-Nsamples.txt","r");
	double d;
	int n;
	int counter = 0;
	int resetter = 0;
	stringstream out;
	string saver ="";
	//load numsamples and imgdim
	while(fscanf(pOut4,"%i",&n) > 0){
		if(counter == 0) imgDim = n;
		if(counter == 1) nsamples = n;
		counter ++;
	}
	
	fclose(pOut4);
	cout<<"loaded numsamples: "<< nsamples<<"....\n";
	cout<<"loaded image dimensions: "<< imgDim<<" ....\n";
	ConvNetInitPrarms(cv1,HiddenLayers,smr,imgDim,nsamples);
	cout<<"net randomized and ready for loading"<<endl;	
	//load weights in Conv layers
	for(int i=0; i < cv1.layer.size(); i++){
		cout<<"layer size = "<<cv1.layer.size()<<"\n";
		cout<<"layer check...."<<cv1.layer[i].W.ATD(0,0)<<"\n";
		counter = 0;
		saver = "wCV";
		out << i;
		saver += out.str();
		loadWeight(cv1.layer[i].W,saver);
		cout<<"loaded "<<saver<<"....\n\n";
		saver = "wgradCV";
		out.str(string());
		out << i;
		saver += out.str();
		loadWeight(cv1.layer[i].Wgrad,saver);
		cout<<"loaded "<<saver<<"....\n\n";
		out.str(string());
	}
	counter = 0;
	 while(fscanf(pOut1,"%lf",&d) > 0){
		//cout << fscanf(pOut1,"%lf",&d) <<"nums pOut1\n";
                if(counter >= 1 && counter %2 == 0)
                        resetter++;

		if(counter %2 == 0){ 
			cv1.layer[resetter].b = d;
			cout<<"loaded b @"<<resetter<<"\n";
		}
                if(counter %2 == 1){
			cv1.layer[resetter].bgrad = d;
			cout<<"loader bgrad @"<<resetter<<"\n";
		}
                counter ++;
		cout<<"count: "<<counter<<endl;
		cout << "loaded cv1dubs pair "<<resetter <<"\n";
         }
	cout<<"loaded cv1dubs....\n";
	fclose(pOut1);	
	
	//load structure of hidden layer network
	for(int i=0; i < HiddenLayers.size(); i++){
		cout<<"loading hidden layers "<<i<<endl;
		counter=0;
		out << i;
		string index = out.str();
		
		saver = "wn";
		saver += index;
		loadWeight(HiddenLayers[i].W,saver);
		
		saver = "b";
		saver += index;
		loadWeight(HiddenLayers[i].b,saver);
		
		saver = "HWgrad";
		saver += index;
		loadWeight(HiddenLayers[i].Wgrad,saver);
		
		saver = "Hbgrad";
		saver += index;
		loadWeight(HiddenLayers[i].bgrad,saver);
		out.str(string());
		cout<<"loaded hidden layer: "<<i<<endl;	
	}
	
	//load soft max regression layer
		
	saver = "wS1";
	loadWeight(smr.Weight,saver);
	
	saver = "bS1";
	loadWeight(smr.b,saver);
	
	saver = "HWgradS1";
	loadWeight(smr.Wgrad,saver);
		
	saver = "HbgradS1";
	loadWeight(smr.bgrad,saver);
	cout<<"loaded Soft max regression layer"<<endl;
	while (fscanf(pOut2,"%lf",&d) > 0)
		smr.cost = d;
	//
	out.clear();	
	fclose(pOut2);
	
}
void 
loadCV(Cvl &cv1){
	
	stringstream out;
	string saver = "";
	for(int i=6; i < cv1.layer.size(); i++){
		cout<<"layer size = "<<cv1.layer.size()<<"\n";
		cout<<"layer check...."<<cv1.layer[i].W.ATD(0,0)<<"\n";
		saver = "wCV";
		out << i;
		saver += out.str();
		loadWeight(cv1.layer[i].W,saver);
		cout<<"loaded "<<saver<<"....\n\n";
		saver = "wgradCV";
		out.str(string());
		out << i;
		saver += out.str();
		loadWeight(cv1.layer[i].Wgrad,saver);
		cout<<"loaded "<<saver<<"....\n\n";
		out.str(string());
	}
	cout<<"CVL load was succesful"<<endl;
}

void 
trainNet(Cvl &cvl,vector<Ntw> &HiddenLayers, SMR &smr,vector<Mat> &trainX, vector<Mat> &testX, Mat &trainY, 
Mat &testY)
{
    long start, end;
    start = clock();
    int iterations = 0;
    cout <<"input the number of iterations you would like to run the network through: ";
    cin >> iterations;
    cout <<endl;
    cout << "check 1" <<endl;
    //readData(trainX, trainY, "trainingSet/train-images.idx3-ubyte", "trainingSet/train-labels.idx1-ubyte", 60000);
    //readData(testX, testY, "trainingSet/t10k-images.idx3-ubyte", "trainingSet/t10k-labels.idx1-ubyte", 10000);
    cout << "check 2" <<endl;
	
    cout<<"Read trainX successfully, including "<<trainX[0].cols * trainX[0].rows<<" features and "<<trainX.size()<<" samples."<<endl;
    cout<<"check 3"<<endl;
    cout<<"Read trainY successfully, including "<<trainY.cols<<" samples"<<endl;
    cout<<"Read testX successfully, including "<<testX[0].cols * testX[0].rows<<" features and "<<testX.size()<<" samples."<<endl;
    cout<<"Read testY successfully, including "<<testY.cols<<" samples"<<endl;

    int nfeatures = trainX[0].rows * trainX[0].cols;
    int imgDim = trainX[0].rows;
    int nsamples = trainX.size();
    
    cout << "check 3" <<endl;
    ConvNetInitPrarms(cvl, HiddenLayers, smr, imgDim, nsamples);
    cout<<"ConvNet initialized"<<endl;
    // Train network using Back Propogation
    batch = nsamples / 100;
    cout<<"concatenating trainX..."<<endl;
    Mat tpX = concatenateMat(trainX);
    cout<<"concatenation succesful..."<<endl;
    double lrate = getLearningRate(tpX);
    cout<<"lrate = "<<lrate<<endl;
    trainNetwork(trainX, trainY, cvl, HiddenLayers, smr, 3e-3, iterations, lrate);

    if(! G_CHECKING){
        // Save the trained kernels, you can load them into Matlab/GNU Octave to see what are they look like.
      /*saveWeight(cvl.layer[0].W, "w0");
        saveWeight(cvl.layer[1].W, "w1");
        saveWeight(cvl.layer[2].W, "w2");
        saveWeight(cvl.layer[3].W, "w3");
        saveWeight(cvl.layer[4].W, "w4");
        saveWeight(cvl.layer[5].W, "w5");
        saveWeight(cvl.layer[6].W, "w6");
        saveWeight(cvl.layer[7].W, "w7");
      */
        // Test use test set
        ///////////////////////////////////////////////////////////////////////////////////
       	saveNetwork(cvl, HiddenLayers, smr, imgDim, nsamples); 
        //Mat result = resultProdict(testX, cvl, HiddenLayers, smr, 3e-3);
        //Mat err(testY);
        //err -= result;
        //int correct = err.cols;
        //for(int i=0; i<err.cols; i++){
        //    if(err.ATD(0, i) != 0) --correct;
       // }
       // cout<<"correct: "<<correct<<", total: "<<err.cols<<", accuracy: "<<double(correct) / (double)(err.cols)<<endl;
	
	////////////////////////////////////////////////////////////////////////////////////
	//forwardPass("bellman.jpg",cvl, HiddenLayers, smr, 3e-3);
    }    
    end = clock();
    cout<<"Totally used time: "<<((double)(end - start)) / CLOCKS_PER_SEC<<" second"<<endl;
}
void testNetwork(vector<Mat> &testX,Mat &testY, Cvl &cvl, vector<Ntw> &HiddenLayers, SMR &smr, double lambda,int &imgDim, 
int &nsamples){
	
	loadNetwork(cvl,HiddenLayers,smr,imgDim,nsamples);
	//CHANGE BELLMAN.JPG
	if(! G_CHECKING){
	Mat result = resultProdict(testX, cvl, HiddenLayers, smr, 3e-3);
        Mat err(testY);
        err -= result;
        int correct = err.cols;
        for(int i=0; i<err.cols; i++){
            if(err.ATD(0, i) != 0) --correct;
        }
        cout<<"correct: "<<correct<<", total: "<<err.cols<<", accuracy: "<<double(correct) / (double)(err.cols)<<endl;

	
	}
	//forwardPass("bellman.jpg",cvl,HiddenLayers,smr,3e-3);	


} 
void enhanceBlack(Mat &image){

        image.convertTo(image, CV_64FC1, 1.0/255, 0);
        for(int i=0; i<image.cols; i++){
                for(int a=0; a<image.rows; a++){
                        //cout<<"intensity at :"<<i<<", "<<a<<" = "<<image.at<double>(i,a)<<"\n";
                        //cout<<image.at<double>(a,i)<<"\n";
			if(image.at<double>(a,i) < .9){
                                image.at<double>(a,i)=0;
                        }
			else
				image.at<double>(a,i)=1;

                }

        }

}


int forwardPass(/*vector<string> path,*/ vector <Mat> &testIms, Mat &retArr){
	
	Cvl cvl;
	vector<Ntw> HiddenLayers;
	SMR smr;
	int imgDim;
	int nsamples;
	loadNetwork(cvl,HiddenLayers,smr,imgDim,nsamples);	
        Mat image;
        vector<Mat> images;
	namedWindow("showIm",WINDOW_AUTOSIZE);
	//CHANGE FOR STRING VECTOR //
	for(int i=0; i < testIms.size(); i++){
	/*	Mat dst;
        	Size size(28,28);
        	resize(testIms[i],dst,size);

        	images.push_back(dst);
		dst.release(); */
		//dst = imread(path[i],0);
		//enhanceBlack(dst);
		images.push_back(testIms[i]);
		//dst.release();
		
	
	}
	Mat result = resultProdict(images,cvl,HiddenLayers,smr,3e-3);
	retArr = result;

	while(true){
		
		int response;
		cout<<"type 1 to test an image index....\n";
		cout<<"type 2 to exit....\n";
		cin >> response;
		if(response == 1){
			int index;
        		cout<<"type the image index 0-"<<images.size()-1<<" : ";
			cin >> index;
			cout<<"\n";
			imshow("showIm",images[index]);	
				
        		if((double) result.ATD(0,index) == 3){
				cout<<"signature detected"<<endl;
			
			}
			else{
				cout<<"not a signature ("<<(double) result.ATD(0,index)<<")"<<endl;
			}
			waitKey(1000);
		}
		if(response == 2){
			break;
		
		}
		else{}
	}
	destroyWindow("showIm");
}

void loadToArr(vector<Mat> &ims, int numIms){

	string end = ".png";
	
	for(int i=1; i < numIms+1; i++){
		Mat image,dst;
		stringstream out;
		
		string path="SigSamples/SigSamples/seg_sig/Sig_";
		if(i < 10){
			
			path+="000";	
		}	
		if(i > 9 && i < 100){
			
			path+="00";
		}
		if(i > 99 && i < 1000){
			
			path+="0";
		
		}
		else{}
		
		//correcting enumeration error in signature files...	
		if(i == 412) i++;
		
		out << i;
		path+= out.str();
		
		if( i ==  354){
			end = ".PNG";
		}
		path+=end;
		
		image = imread(path,0);
		
		enhanceBlack(image);
		
		Size size(60,60);
		resize(image,dst,size);
		
		ims.push_back(dst);
		dst.release();
		
	
	}
	cout<<numIms<<" images loaded into array.."<<endl;


}
void fillArrNegatives(vector<Mat> &arr, int numEmpties){

	for(int i=0; i < numEmpties; i++){
		Mat canvas = Mat::ones(Size(60,60), CV_64FC1);
		arr.push_back(canvas);
		Mat bcanvas = Mat::ones(Size(60,60), CV_64FC1);
		arr.push_back(bcanvas);
	}

}


int readFiles(vector<Mat> &signatures, int numImages){
	
	loadToArr(signatures,numImages);
	//fillArrNegatives(signatures,(numImages-1)/2);
	cout<<"size = "<<signatures.size()<<endl;

	return 0;

}


void drawOn(Mat &inImage, Mat image, int start_x, int start_y){

    //for every x in pixel array
    for( int i=0; i < image.cols; i++){

        //for every y in pixel array
        for(int a=0; a < image.rows; a++){

                inImage.at<Vec3b>(start_y+a,start_x+i)[0] = image.at<Vec3b>(a,i)[0];
                inImage.at<Vec3b>(start_y+a,start_x+i)[1] = image.at<Vec3b>(a,i)[1];
                inImage.at<Vec3b>(start_y+a,start_x+i)[2] = image.at<Vec3b>(a,i)[2];
        }

    }

}

void windowMakeGreen(Mat &image){

    //for every x in pixel array
    for( int i=0; i < image.cols; i++){

        //for every y in pixel array
        for(int a=0; a < image.rows; a++){
                if(image.at<Vec3b>(a,i)[1] < 100)
                        image.at<Vec3b>(a,i)[1] += 100 ;
                else
                        image.at<Vec3b>(a,i)[1] = 255 ;

        }

    }
}

void windowMakeRed(Mat &image){

    //for every x in pixel array
    for( int i=0; i < image.cols; i++){

        //for every y in pixel array
        for(int a=0; a < image.rows; a++){
                if(image.at<Vec3b>(a,i)[0] < 100)
                        image.at<Vec3b>(a,i)[2] += 100 ;
                else
                        image.at<Vec3b>(a,i)[2] = 255 ;

        }

    }
}


Mat createImSlice(Mat inImage,  int w_width, int w_height,int start_x, int start_y){

    Size size(w_width,w_height);
    Mat image = Mat::ones(size,CV_8UC3);

    //for every x in pixel array
    for( int i=0; i < w_width; i++){

        //for every y in pixel array
        for(int a=0; a < w_height; a++){

                image.at<Vec3b>(a,i)[0] = inImage.at<Vec3b>(start_y+a,start_x+i)[0];
                image.at<Vec3b>(a,i)[1] = inImage.at<Vec3b>(start_y+a,start_x+i)[1];
                image.at<Vec3b>(a,i)[2] = inImage.at<Vec3b>(start_y+a,start_x+i)[2];
        }

    }

   return image;

}

Mat createImSliceBlack(Mat inImage,  int w_width, int w_height,int start_x, int start_y){

    Size size(w_width,w_height);
    Mat image = Mat::zeros(size,CV_64FC1);
    image.convertTo(image,CV_64FC1,1.0/255,0);
    //for every x in pixel array
    for(int i=0; i < w_width; i++){
        //for every y in pixel array
        for(int a=0; a < w_height-1; a++){
                image.at<double>(a,i) = inImage.at<double>(start_y+a,start_x+i);
		if(image.at<double>(a,i) < .9)
			image.at<double>(a,i) = 0;
		else
			image.at<double>(a,i) = 1;
        }

    }

   return image;

}
void segmentNSave(Mat image, int xMax, int yMax, int segSizeX,int segSizeY){
	int count=0;
	for(int x=0; x < xMax - segSizeX; x+=20){
                for(int y=0; y < yMax - segSizeY; y+=20){
			stringstream out;
			string saver = "SigSamples/SigSamples/neg_sig/neg_sig_";
			string end = ".png";
			out << count;
			saver += out.str();
			saver += end;
				
                        Mat imageSlice;
                        imageSlice = createImSlice(image,segSizeX,segSizeY,x,y);
			
			imwrite(saver,imageSlice);
				
                        imageSlice.release();
			cout<<saver<<" saved ..."<<endl;
			out.str(string());
			count++;
                       

                }

        }

}

void loadNegsToArr(vector<Mat> &arr, int numIms){
	cout<<"load negs started......"<<endl;
	for(int i=0; i < numIms; i++){
		
		stringstream out;
                string saver = "SigSamples/SigSamples/neg_sig/neg_sig_";
                string end = ".png";
                out << i;
                saver += out.str();
                saver += end;
		//cout<<"check 1"<<endl;
		Mat saveIm,dst;
		
		saveIm = imread(saver,0);
		enhanceBlack(saveIm);
		
		Size size(60,60);
		resize(saveIm,dst,size);
		cout<<"loaded image = "<<saver<<endl;
		//namedWindow("CHECKING",WINDOW_AUTOSIZE);
		//imshow("CHECKING",dst);
		//waitKey(50);	
		arr.push_back(dst);
		dst.release();
		out.str(string());
	}



}

void scanImage(Mat image, Mat blackIm){
	
	cout<<"image type = "<<type2str(image.type())<<endl;
	
	cout<<"blackIm type = "<<type2str(blackIm.type())<<endl;
		
	Mat redrawIm = Mat::ones(Size(image.cols,image.rows),CV_8UC3);
	vector<Mat> inputIms;
	vector<Mat> originalIms;
	Mat returnArr;

	cout<<"CHECK 1"<<endl;

	int checker = 0;
	
	
	
	blackIm.convertTo(blackIm,CV_64FC1,1.0/255,0);
	//Prelim val 200
	int splitterX = 200;
	
	//Prelim val 100
	int splitterY = 100;
	for(int x=0; x < image.cols - splitterX; x += splitterX/10){
		for(int y=0; y < image.rows - splitterY; y += splitterY/10){
			
			Mat imageSlice,dst,blackSlice;
			imageSlice = createImSlice(image,splitterX,splitterY,x,y);
			originalIms.push_back(imageSlice);
			
			blackSlice = createImSliceBlack(blackIm,splitterX,splitterY,x,y);
				
			Size size(60,60);		

			resize(blackSlice,dst,size);	
			inputIms.push_back(dst);
		
			imageSlice.release();
			blackSlice.release();
			dst.release();

			if(x % 50 == 0)
				cout<<"##";
		
		} 
	
	}
	cout<<"documented segmented into "<<inputIms.size()<<" snippets, running through net..."<<endl;
	forwardPass(inputIms,returnArr);
	int count=0;
	namedWindow("IMAGE",WINDOW_NORMAL);
	resizeWindow("IMAGE",800,1000);
	imshow("IMAGE",image);
	cout<<"CHECK 2"<<endl;	
	
	for(int x=0; x < image.cols - splitterX; x += splitterX/10){
                for(int y=0; y < image.rows - splitterY; y += splitterY/10){

			drawOn(redrawIm,originalIms[count],x,y);
			count++;
		}
	}
	
	count=0;
		
	for(int x=0; x < image.cols - splitterX; x += splitterX/10){
                for(int y=0; y < image.rows - splitterY; y += splitterY/10){
			/*
                        if((double) returnArr.ATD(0,count) == 2){
                                windowMakeRed(originalIms[count]);
                                drawOn(redrawIm,originalIms[count],x,y);
                        }
			*/
			if((double) returnArr.ATD(0,count) == 3){
				windowMakeGreen(originalIms[count]);
				drawOn(redrawIm,originalIms[count],x,y);
			
			}
	
                        count++;
                }
        }

	
	namedWindow("SCANNED IMAGE",WINDOW_NORMAL);
	resizeWindow("SCANNED IMAGE",800,1000);
	imshow("SCANNED IMAGE",redrawIm);
	waitKey(0);
	
	
}




int 
main(){
	
	Cvl cvl;
	vector<Ntw> HiddenLayers;
	SMR smr;
	vector<Mat> trainX;
	vector<Mat> resizeX;
	vector<Mat> testX;
	vector<Mat> resizeY;
	vector<Mat> signatures;
	Mat trainY, testY, resizeTestX, resizeTestY,sigLabels,saveArr;
	
	int imgDim;
	int nsamples;
	readData(trainX, trainY, "trainingSet/train-images.idx3-ubyte", "trainingSet/train-labels.idx1-ubyte", 60000);
	readData(testX, testY, "trainingSet/t10k-images.idx3-ubyte", "trainingSet/t10k-labels.idx1-ubyte", 10000);
	
	
	//TESTING DIFFERENT SIZED DATA//
	for(int i=0; i < trainX.size(); i++){
		Mat dst;
		
        	Size size(28,28);
        	resize(trainX[i],dst,size);
        	resizeX.push_back(dst);
		dst.release();
	}
	for(int i=0; i < testX.size(); i++){
                Mat dst;

                Size size(28,28);
                resize(testX[i],dst,size);
                resizeY.push_back(dst);
                dst.release();
        }
	for(int i=0; i < 8; i++)
		readFiles(signatures,466);
	cout<<"check 1"<<endl;
	
	loadNegsToArr(signatures,2720);
	
	sigLabels = Mat::zeros(1,6448,CV_64FC1);
	for(int i=0; i < 6448; i++){
		if(i < 3728)	
			sigLabels.ATD(0,i) = (double) 3;
		else{
			if( i % 2 != 0)
				sigLabels.ATD(0,i) = (double) 2;
			if( i % 2 == 0)
				sigLabels.ATD(0,i) = (double) 2;
		}
	
	}
	/*
	readFiles(signatures,466);
	 for(int i=969; i < 1938; i++){
                if(i < 969+465)
                        sigLabels.ATD(0,i) = (double) 3;
                else{
                        if( i % 2 != 0)
                                sigLabels.ATD(0,i) = (double) 2;
                        if( i % 2 == 0)
                                sigLabels.ATD(0,i) = (double) 2;
                }

        }
	*/
		
        	        	        			
        			
	while(true){
		int response;
		cout<<"Enter 1 to train network\n";
		cout<<"Enter 2 to load network\n";
		cout<<"Enter 3 for testing ...\n";
		cout<<"Enter 4 to run forward pass on images...\n";
		cout<<"Enter 5 to scan a document for signatures...\n";
		cout<<"Enter 6 to segment part of an image and save the segments...\n";
		cout<<"Enter 7 to exit....\n";
	
		cin >> response;
		//ERASE WHEN NOT USING GDB//
	
		if(response == 1){
			trainNet(cvl,HiddenLayers,smr,signatures,resizeY,sigLabels,testY);
			
		}
		if(response == 2){
			loadNetwork(cvl,HiddenLayers,smr,imgDim,nsamples);
		}
		if(response == 3){
			cout<<"loading numsamples and image dimensions....."<<endl;
			FILE *pOut4 = fopen("imgDim-Nsamples.txt","r");
			int counter = 0;
			int n;
			while(fscanf(pOut4,"%i",&n) > 0){
				if(counter == 0) imgDim = n;
				if(counter == 1) nsamples = n;
				counter ++;
			}
			counter = 0;
			fclose(pOut4);
			cout<<"numsamples loaded: "<<nsamples<<"....\n";
			cout<<"imgdim loaded: "<<imgDim<<"....\n";
			ConvNetInitPrarms(cvl,HiddenLayers,smr,imgDim,nsamples);
			cout<<"architecture randomnized and ready for setting....\n";
			cout<< "testing load Cvl...." << endl;
			//loading CV
			loadCV(cvl);
			cout<<"testing indexes ....\n";
			//for( int i=0; i < cvl.layer.size(); i++){
			//	cout<<" last index = "<<cvl.layer[i].W.ATD(12,12)<<endl;
			//}
			//add testing methods here if neccessary
		
		}
		vector<string> arr;
		if(response == 4){
			forwardPass(signatures,saveArr);
		
		}
		if(response == 5){
			Mat scanDoc;
			Mat blackDoc;
			scanDoc.convertTo(scanDoc,CV_64FC3);
			string path;
			
			//namedWindow("ORIGINAL IM",WINDOW_AUTOSIZE);
			//cout<<"type the name of the document to scan: ";
			//cin >> path;
			scanDoc = imread("SigSamples/SigSamples/SigSample_11.png",1);
			blackDoc = imread("SigSamples/SigSamples/SigSample_11.png",0);
			//namedWindow("CHECKING",WINDOW_AUTOSIZE);
			//imshow("CHECKING",scanDoc);
			waitKey(50);	
			//Rotate scanDoc 90 clockwise
			Point2f src_center(scanDoc.cols/2.0F, scanDoc.rows/2.0F);
			Mat rot_mat = getRotationMatrix2D(src_center, 270, 1.0);
			Mat dst;
			warpAffine(scanDoc, dst, rot_mat, scanDoc.size());
			
			//Rotate blackDoc 90 clockwise
			Point2f src_centerB(blackDoc.cols/2.0F, blackDoc.rows/2.0F);
			Mat rot_matB = getRotationMatrix2D(src_centerB, 270, 1.0);
			Mat dstB;
			warpAffine(blackDoc, dstB, rot_matB, blackDoc.size());
				
			//imshow("ORIGINAL IM",scanDoc);
			//waitKey(1000);
			scanImage(dst,dstB);

		}
		if(response == 6){
			Mat segmentIm;
			segmentIm = imread("SigSamples/SigSamples/Sample_3-0_LR.png",1);
			segmentNSave(segmentIm,segmentIm.cols,600,200,100);
		
		}
		if(response == 7)
			break;
	
	}
	return 0;
}
