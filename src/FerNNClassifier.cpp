/*
 * FerNNClassifier.cpp
 *
 *  Created on: Jun 14, 2011
 *      Author: alantrrs
 */

#include <FerNNClassifier.h>

using namespace cv;

void FerNNClassifier::read(const FileNode& file){
  ///Classifier Parameters
  valid = (float)file["valid"];
  ncc_thesame = (float)file["ncc_thesame"];
  nstructs = (int)file["num_trees"];
  structSize = (int)file["num_features"];
  thr_fern = (float)file["thr_fern"];
  thr_nn = (float)file["thr_nn"];
  thr_nn_valid = (float)file["thr_nn_valid"];
}

void FerNNClassifier::prepare(const vector<Size>& scales){
  //Initialize test locations for features
  int totalFeatures = nstructs*structSize;
  features = vector<vector<Feature> >(scales.size(),vector<Feature> (totalFeatures));
  RNG& rng = theRNG();
  float x1f,x2f,y1f,y2f;
  int x1, x2, y1, y2;
  for (int i=0;i<totalFeatures;i++){
      x1f = (float)rng;
      y1f = (float)rng;
      x2f = (float)rng;
      y2f = (float)rng;
      for (int s=0;s<scales.size();s++){
          x1 = x1f * scales[s].width;
          y1 = y1f * scales[s].height;
          x2 = x2f * scales[s].width;
          y2 = y2f * scales[s].height;
          features[s][i] = Feature(x1, y1, x2, y2);
      }

  }
  //Negative Threshold
  thrN = 0.5*nstructs;
  //Initialize Posteriors
  for (int i = 0; i<nstructs; i++) {
      posteriors.push_back(vector<float>(pow(2.0,structSize), 0));
      pCounter.push_back(vector<int>(pow(2.0,structSize), 0));
      nCounter.push_back(vector<int>(pow(2.0,structSize), 0));
  }
/*
  for (int i = 0; i<nstructs; i++) {
      for (int j = 0; j < posteriors[i].size(); j++) {
          posteriors[i].at(j) = 0;
          pCounter[i].at(j) = 0;
          nCounter[i].at(j) = 0;
      }
  }
*/
}

void FerNNClassifier::getFeatures(const cv::Mat& image,const cv::Rect& box, int scale_idx,vector<pair<vector<int>,int> >& ferns,int label){
  vector<int> fern(nstructs);
  int leaf;
  for (int t=0;t<nstructs;t++){
      leaf=0;
      for (int f=0; f<structSize; f++){
          leaf = (leaf << 1) + features[scale_idx][t*nstructs+f](image(box));
      }
      fern[t]=leaf;
  }
  ferns.push_back(make_pair(fern,label));
}

float FerNNClassifier::measure_forest(vector<int> fern) {
  float votes = 0;
  for (int i = 0; i < nstructs; i++) {
      votes += posteriors[i][fern[i]];
  }
  //printf("votes:%f",votes);
  return votes;
}

void FerNNClassifier::update(vector<int> fern, int C, int N) {
  for (int i = 0; i < nstructs; i++) {
      int idx = fern[i];
      //printf("[%d] C:%d ",idx,C);
      (C==1) ? pCounter[i][idx] += N : nCounter[i][idx] += N;
      if (pCounter[i][idx]==0) {
          posteriors[i][idx] = 0;
      } else {
          posteriors[i][idx] = ((float) (pCounter[i][idx])) / (pCounter[i][idx] + nCounter[i][idx]);
      }
  }
}

void FerNNClassifier::trainF(const vector<pair<vector<int>,int> >& ferns,int resample){
  // Conf = function(2,X,Y,Margin,Bootstrap,Idx)
  //                 0 1 2 3      4         5
  //  double *X     = mxGetPr(prhs[1]); -> ferns[i].first
  //  int numX      = mxGetN(prhs[1]);  -> ferns.size()
  //  double *Y     = mxGetPr(prhs[2]); ->ferns[i].second
  //  double thrP   = *mxGetPr(prhs[3]) * nTREES; ->threshold*nstructs
  //  int bootstrap = (int) *mxGetPr(prhs[4]); ->resample
  //TODO: Check consistency
  thrP = thr_fern*nstructs;
  // int step = numX / 10;
  for (int j = 0; j < resample; j++) { // for (int j = 0; j < bootstrap; j++) {
      for (int i = 0; i < ferns.size(); i++){ //for (int i = 0; i < step; i++) {
          //for (int k = 0; k < 10; k++) {
          // int I = k*step + i;//box index
          //double *x = X+nTREES*I; //tree index
          if(ferns[i].second==1){     //if (Y[I] == 1) {
              if(measure_forest(ferns[i].first)<=thrP)//if (measure_forest(x) <= thrP)
                update(ferns[i].first,1,1);//    update(x,1,1);
          }else{
              if (measure_forest(ferns[i].first) >= thrN)// if (measure_forest(x) >= thrN)
                update(ferns[i].first,0,1);//      update(x,0,1);
          }
      }
  }
  /*#IF DEBUG
  for (int j=0;j<posteriors.size();j++){
      printf("posterior[%d]=\n",j);
      for (int i = 0; i<posteriors[j].size();i++){
          if (pCounter[j][i]>0 || nCounter[j][i]>0)
            printf("posterior: %f positive:%d      negative:%d\n",posteriors[j][i],pCounter[j][i],nCounter[j][i]);
      }
  }
  */
}

void FerNNClassifier::trainNN(const vector<cv::Mat>& nn_examples){
  //TODO:Check consistency and optimize code
  //  function tld = tldTrainNN(pEx,nEx,tld)
  //  nP = size(pEx,2); % get the number of positive example
  //  nN = size(nEx,2); % get the number of negative examples
  //  x = [pEx,nEx];
  //  y = [ones(1,nP), zeros(1,nN)];
  //  % Permutate the order of examples
  //  idx = randperm(nP+nN); %
  //  if ~isempty(pEx)
  //      x   = [pEx(:,1) x(:,idx)]; % always add the first positive patch as the first (important in initialization)
  //      y   = [1 y(:,idx)];
  //  end
  float conf;
  vector<int> y(nn_examples.size(),0);
  y[0]=1;
  vector<int> isin;
  for (int i=0;i<nn_examples.size();i++){  //     for i = 1:length(y)
      conf= NNConf(nn_examples[i],isin);   //[conf1,dummy5,isin] = tldNN(x(:,i),tld); % measure Relative similarity
      if (y[i]==1 && conf<=thr_nn){     //  if y(i) == 1 && conf1 <= tld.model.thr_nn % 0.65
          if (isin[1]<0){       //  if isnan(isin(2))
              pEx = vector<Mat>(1,nn_examples[i]); //  tld.pex = x(:,i);
              continue; //  continue;
          }              //  end
          pEx.insert(pEx.begin()+isin[1],nn_examples[i]); //  tld.pex = [tld.pex(:,1:isin(2)) x(:,i) tld.pex(:,isin(2)+1:end)]; % add to model
      } //  end
      if(y[i]==0 && conf>0.5) //  if y(i) == 0 && conf1 > 0.5
        nEx.push_back(nn_examples[i]);  //  tld.nex = [tld.nex x(:,i)];
  }//  end
}//  end


float FerNNClassifier::NNConf(const Mat& example, vector<int>& isin){
  //function [conf1,conf2,isin] = tldNN(x,tld)
  //% 'conf1' ... full model (Relative Similarity)
  //% 'conf2' ... validated part of model (Conservative Similarity)
  //% 'isnin' ... inside positive ball, id positive ball, inside negative ball
  isin=vector<int>(3,-1);//isin = nan(3,size(x,2)); -
  if (pEx.empty()){ //if isempty(tld.pex) % IF positive examples in the model are not defined THEN everything is negative
     float conf = 0; //    conf1 = zeros(1,size(x,2));
     return conf;
  }
  if (nEx.empty()){ //if isempty(tld.nex) % IF negative examples in the model are not defined THEN everything is positive
    float conf = 1;   //    conf1 = ones(1,size(x,2));
    return conf;
  }
  //conf1 = nan(1,size(x,2));
  //conf2 = nan(1,size(x,2));
  //for i = 1:size(x,2) % fore every patch that is tested
  Mat ncc;
  float nccP, maxP=0;
  bool anyP=false;
  int maxPidx;
  float nccN, maxN=0;
  bool anyN=false;
  for (int i=0;i<pEx.size();i++){
      matchTemplate(pEx[i],example,ncc,CV_TM_CCOEFF_NORMED);//    nccP = distance(x(:,i),tld.pex,1); % measure NCC to positive examples
      nccP=(float)*ncc.data;
      matchTemplate(nEx[i],example,ncc,CV_TM_CCOEFF_NORMED);//    nccN = distance(x(:,i),tld.nex,1); % measure NCC to negative examples
      nccN=(float)*ncc.data;
      if (nccP>ncc_thesame)
        anyP=true;
      if (nccN>ncc_thesame)
        anyN=true;
      if(nccP > maxP){
        maxP=nccP;
        maxPidx = i;
      }
      if(nccN > maxN)
        maxN=nccN;
  }
  //    % set isin
  if (anyP) isin[0]=1;//    if any(nccP > tld.model.ncc_thesame), isin(1,i) = 1;  end % IF the query patch is highly correlated with any positive patch in the model THEN it is considered to be one of them
  isin[1]=maxPidx;//    [dummy6,isin(2,i)] = max(nccP); % get the index of the maximall correlated positive patch
  if (anyN) isin[2]=1;//    if any(nccN > tld.model.ncc_thesame), isin(3,i) = 1;  end % IF the query patch is highly correlated with any negative patch in the model THEN it is considered to be one of them
  //
  //    % measure Relative Similarity
  float dN=1-maxN;//    dN = 1 - max(nccN);
  float dP=1-maxP;//    dP = 1 - max(nccP);
  return (float)dN/(dN+dP);//    conf1(i) = dN / (dN + dP);
  //    % measure Conservative Similarity
  //    maxP = max(nccP(1:ceil(tld.model.valid*size(tld.pex,2))));
  //    dP = 1 - maxP;
  //    conf2(i) = dN / (dN + dP);
  //end
}
void FerNNClassifier::evaluateTh(const vector<pair<vector<int>,int> >& nXT,const vector<cv::Mat>& nExT){
//TODO! Estimate thresholds on validation set------------------------------------
//  % Fern
//  conf_fern = fern(3,nX2);
//  tld.model.thr_fern = max(max(conf_fern)/tld.model.num_trees,tld.model.thr_fern);
float fconf;
  for (int i=0;i<nXT.size();i++){
    fconf = (float) measure_forest(nXT[i].first)/nstructs;
    if (fconf>thr_fern)
      thr_fern=fconf;
}
//  % Nearest neighbor
//  conf_nn = tldNN(nEx2,tld);
//  tld.model.thr_nn = max(tld.model.thr_nn,max(conf_nn));
//  tld.model.thr_nn_valid = max(tld.model.thr_nn_valid,tld.model.thr_nn);

  vector <int> isin;
  float conf;
  for (int i=0;i<nExT.size();i++){
      conf=NNConf(nExT[i],isin);
      if (conf>thr_nn)
        thr_nn=conf;
  }
  if (thr_nn>thr_nn_valid)
    thr_nn_valid = thr_nn;
}

