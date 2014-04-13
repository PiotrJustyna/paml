/* evolver.c
   Copyright, Ziheng Yang, April 1995.

     cc -fast -o evolver evolver.c tools.o eigen.o -lm
     evolver
*/

/* Do not define both CodonNSSites & CodonNSbranches */

/*
#define CodonNSbranches  1
#define CodonNSsites     1
*/




#include "paml.h"

#define NS            5000
#define NBRANCH       (NS*2-2)
#define NCODE         64
#define NCATG         40

struct CommonInfo {
   char *z[2*NS-1], spname[NS][10], daafile[96];
   int ns,ls,npatt,*fpatt,np,ntime,ncode,clock,rooted,model,icode,cleandata;
   int seqtype, *pose, ncatG, npi0;
   double kappa, omega, alpha, pi[64],*rates, *lkl, daa[20*20], pi_sqrt[NCODE];
   double freqK[NCATG],rK[NCATG];
}  com;
struct TREEB {
   int nbranch, nnode, root, branches[NBRANCH][2];
}  tree;
struct TREEN {
   int father, nson, sons[NS], ibranch, label;
   double branch, divtime, omega, *lkl, daa[20*20];
}  *nodes;

extern char BASEs[];
extern int GeneticCode[][64];
int LASTROUND=0; /* not used */

#define NODESTRUCTURE
#define BIRTHDEATH
#include "treesub.c"
#include "treespace.c"

void TreeDistances (FILE* fout);
void Simulate (char*ctlf);
void MakeSeq(char*z, int ls);
int EigenQbase(double rates[], double pi[], 
    double Root[],double U[],double V[],double Q[]);
int EigenQcodon (int getstats, double kappa,double omega,double pi[],
    double Root[], double U[], double V[], double Q[]);
int EigenQaa(double pi[],double Root[], double U[], double V[],double Q[]);
int between_f_and_x(void);

char *MCctlf0[]={"MCbase.dat","MCcodon.dat","MCaa.dat"};
char *seqf[2]={"mc.paml", "mc.paup"};

enum {JC69, K80, F81, F84, HKY85, TN93, REV} BaseModels;
char *basemodels[]={"JC69","K80","F81","F84","HKY85","TN93","REV"};
enum {Poisson, EqualInput, Empirical, Empirical_F} AAModels;
char *aamodels[]={"Poisson", "EqualInput", "Empirical", "Empirical_F"};


double PMat[NCODE*NCODE],U[NCODE*NCODE],V[NCODE*NCODE],Root[NCODE];
static double Qfactor=-1, Qrates[5];  /* Qrates[] hold kappa's for nucleotides */


int main(int argc, char*argv[])
{
   char *MCctlf=NULL, outf[]="evolver.out";
   int i, option=6, ntree=1,rooted, BD=0;
   double bfactor=1, birth=-1,death=-1,sample=-1,mut=-1, *space;
   FILE *fout=fopen(outf,"w");

   printf("\nEVOLVER (in %s)\n",  VerStr);
   if(fout==NULL) { printf("cannot create %s\n",outf); exit(-1); }
   else             printf("Results for options 1-4 & 8 go into %s\n",outf);
   if(argc!=1 && argc!=3) {
      puts("Usage: \n\tevolver \n\tevolver option# MyDataFile"); exit(-1); 
   }
   if(argc==3) {
      sscanf(argv[1],"%d",&option);
      MCctlf=argv[2];
      if(option<5 || option>7) error2("command line option not right.");
   }
   else {
      for(; ;) {
         printf("\n\t(1) Get random UNROOTED trees?\n"); 
         printf("\t(2) Get random ROOTED trees?\n"); 
         printf("\t(3) List all UNROOTED trees into file trees?\n");
         printf("\t(4) List all ROOTED trees into file trees?\n");
         printf("\t(5) Simulate nucleotide data sets (use %s)?\n",MCctlf0[0]);
         printf("\t(6) Simulate codon data sets      (use %s)?\n",MCctlf0[1]);
         printf("\t(7) Simulate amino acid data sets (use %s)?\n",MCctlf0[2]);
         printf("\t(8) Calculate identical bi-partitions between trees?\n");
         printf("\t(0) Quit?\n");
#if defined (CodonNSbranches)
         option=6;  MCctlf="MCcodonNSbranches.dat";
#elif defined (CodonNSsites)
         option=6;  MCctlf="MCcodonNSsites.dat";
#else 
         scanf("%d", &option);
#endif
         if(option==0) exit(0);
         if(option>=5 && option<=7) break;
         if(option<5)  { printf ("No. of species: ");   scanf ("%d", &com.ns); }
         if(com.ns>NS) error2 ("Too many species.  Raise NS.");
         if((space=(double*)malloc(10000*sizeof(double)))==NULL) error2("oom");
         rooted=!(option%2);
         if (option<3) {
            printf("\nnumber of trees & random number seed? ");
            scanf("%d%d",&ntree,&i);  SetSeed(i);
            printf ("Want branch lengths from the birth-death process (0/1)? ");
            scanf ("%d", &BD);
         }
         if(option<=4) {
            i=(com.ns*2-1)*sizeof(struct TREEN);
            if((nodes=(struct TREEN*)malloc(i))==NULL) error2("oom");
         }
         switch (option) {
         case(1):
         case(2):
            if(BD) {
               printf ("\nbirth rate, death rate, sampling fraction, and ");
               printf ("mutation rate (tree height)?\n");
               scanf ("%lf%lf%lf%lf", &birth, &death, &sample, &mut);
            }
            for(i=0;i<ntree;i++) {
               RandomLHistory (rooted, space);
               if(BD)  BranchLengthBD (1, birth, death, sample, mut);
               if(com.ns<20&&ntree<10) { OutaTreeN(F0,0,BD); puts("\n"); }
               OutaTreeN(fout,0,BD);  FPN(fout);
            }
            /* for (i=0; i<com.ns-2-!rooted; i++) Ib[i]=(int)((3.+i)*rndu());
               MakeTreeIb (com.ns, Ib, rooted); */
            break;
         case(3):
         case(4): 
            ListTrees(fout,com.ns,rooted);
            break;
         case(8):  TreeDistances(fout);      break;
         case(9):  between_f_and_x();                break;
         default:  exit(0);
         }
      }
   }
   com.seqtype=option-5;  /* 0, 1, 2 for bases, codons, & amino acids */
   Simulate(MCctlf?MCctlf:MCctlf0[option-5]);
   return(0);
}


int between_f_and_x (void)
{
/* this helps with the exponential transform for frequency parameters */
   int i,n,fromf=0;
   double x[100];

   for(;;) {
      printf("\ndirection (0:x=>f; 1:f=>x; -1:end)  &  #classes? ");
      scanf("%d",&fromf);    
      if(fromf==-1) return(0);
      scanf("%d", &n);  if(n>100) error2("too many classes");
      printf("input the first %d values for %s? ",n-1,(fromf?"f":"x"));
      FOR(i,n-1) scanf("%lf",&x[i]);
      x[n-1]=(fromf?1-sum(x,n-1):0);
      f_and_x(x, x, n, fromf, 1);
      matout(F0,x,1,n);
   }
}

void TreeDistances(FILE* fout)
{
   int i,j,ntree, k,*nib, parti2B[NS], nsame, IBsame[NS],nIBsame[NS], lparti=0;
   char treef[64], *partition;
   FILE *ftree;
   double psame, mp, vp;

   puts("\nNumber of identical bi-partitions between trees.\nTree file name?");
   scanf ("%s", treef);
   if ((ftree=fopen (treef,"r"))==NULL) error2 ("Tree file not found.");
   fscanf (ftree, "%d%d", &com.ns, &ntree);
   i=(com.ns*2-1)*sizeof(struct TREEN);
   if((nodes=(struct TREEN*)malloc(i))==NULL) error2("oom");

   if(ntree<2) error2("ntree");
   printf ("\n%d species, %d trees\n", com.ns, ntree);
   puts("\n\t1: first vs. rest?\n\t2: all pairwise comparsons?\n");
   scanf("%d", &k);

   lparti=(com.ns-1)*com.ns*sizeof(char);
   i=(k==1?2:ntree)*lparti;
   printf("\n%d bytes of space requested.\n", i);
   partition=(char*)malloc(i);
   nib=(int*)malloc(ntree*sizeof(int));
   if (partition==NULL || nib==NULL) error2("out of memory");

   if(k==2) {    /* pairwise comparisons */
     fputs("Number of identical bi-partitions in pairwise comparisons\n",fout);
      for (i=0; i<ntree; i++) {
         ReadaTreeN (ftree, &k, 1); 
         nib[i]=tree.nbranch-com.ns;
         BranchPartition(partition+i*lparti, parti2B);
      }
      for (i=0; i<ntree; i++,FPN(F0),FPN(fout)) {
         printf("%2d (%2d):", i+1,nib[i]);
         fprintf(fout,"%2d (%2d):", i+1,nib[i]);
         for (j=0; j<i; j++) {
            nsame=NSameBranch(partition+i*lparti,partition+j*lparti, 
                  nib[i],nib[j],IBsame);
            fprintf(fout," %2d", nsame);
         }
      }
   }
   else {  /* first vs. others */
      ReadaTreeN (ftree, &k, 1); 
      nib[0]=tree.nbranch-com.ns;
      if (nib[0]==0) error2("1st tree is a star tree..");
      BranchPartition (partition, parti2B);
      fputs ("Comparing the first tree with the others\nFirst tree:\n",fout);
      OutaTreeN(fout,0,0);  FPN(fout);  OutaTreeB(fout);  FPN(fout); 
      fputs ("\nInternal branches in the first tree:\n",fout);
      FOR(i,nib[0]) { 
         k=parti2B[i];
         fprintf(fout,"%3d (%2d..%-2d): ( ",
            i+1,tree.branches[k][0]+1,tree.branches[k][1]+1);
         FOR(j,com.ns) if(partition[i*com.ns+j]) fprintf(fout,"%d ",j+1);
         fputs(")\n",fout);
      }
      if(nodes[tree.root].nson<=2) 
         fputs("\nRooted tree, results may not be correct.\n",fout);
      fputs("\nCorrect internal branches compared with the 1st tree:\n",fout);
      FOR(k,nib[0]) nIBsame[k]=0;
      for (i=1,mp=vp=0; i<ntree; i++,FPN(fout)) {
         ReadaTreeN (ftree, &k, 1); 
         nib[1]=tree.nbranch-com.ns;
         BranchPartition (partition+lparti, parti2B);
         nsame=NSameBranch (partition,partition+lparti, nib[0],nib[1],IBsame);

         psame=nsame/(double)nib[0];
         FOR(k,nib[0]) nIBsame[k]+=IBsame[k];
         fprintf(fout,"1 vs. %3d: %4d: ", i+1,nsame);
         FOR(k,nib[0]) if(IBsame[k]) fprintf(fout," %2d", k+1);
         printf("1 vs. %5d: %6d/%d  %10.4f\n", i+1,nsame,nib[0],psame);
         vp += square(psame - mp)*(i-1.)/i;
         mp=(mp*(i-1.) + psame)/i;
      }
      vp=(ntree<=2 ? 0 : sqrt(vp/((ntree-1-1)*(ntree-1.))));
      fprintf(fout,"\nmean and S.E. of proportion of identical partitions\n");
      fprintf(fout,"between the 1st and all the other %d trees ", ntree-1);
      fprintf(fout,"(ignore these if not revelant):\n %.4f +- %.4f\n", mp, vp);
      fprintf(fout,"\nNumbers of times, out of %d, ", ntree-1);
      fprintf(fout,"interior branches of tree 1 are present");
      fputs("\n(This may be bootstrap support for nodes in tree 1)\n",fout);
      FOR(k,nib[0]) { 
         i=tree.branches[parti2B[k]][0]+1;  j=tree.branches[parti2B[k]][1]+1; 
         fprintf(fout,"%3d (%2d..%-2d): %6d (%5.1f%%)\n",
            k+1,i,j,nIBsame[k],nIBsame[k]*100./(ntree-1.));
      }
   }
   free(partition);  free(nodes); free(nib);  fclose(ftree);
}




int EigenQbase(double rates[], double pi[], 
    double Root[],double U[],double V[],double Q[])
{
/* Construct the rate matrix Q[] for nucleotide model REV.
*/
   int i,j,k;
   double mr;

   zero (Q, 16);
   for (i=0,k=0; i<3; i++) for (j=i+1; j<4; j++)
      if (i*4+j!=11) Q[i*4+j]=Q[j*4+i]=rates[k++];
   for (i=0,Q[3*4+2]=Q[2*4+3]=1; i<4; i++) FOR (j,4) Q[i*4+j] *= pi[j];
   for (i=0,mr=0; i<4; i++) 
      { Q[i*4+i]=0; Q[i*4+i]=-sum(Q+i*4, 4); mr-=pi[i]*Q[i*4+i]; }
   abyx (1/mr, Q, 16);

   eigenQREV(Q, com.pi, com.pi_sqrt, 4, com.npi0, Root, U, V);
   return (0);
}


static double freqK_NS=-1;

int EigenQcodon (int getstats, double kappa,double omega,double pi[],
    double Root[], double U[], double V[], double Q[])
{
/* Construct the rate matrix Q[].
   64 codons are used, and stop codons have 0 freqs.
*/
   int n=com.ncode, i,j,k, c[2],ndiff,pos=0,from[3],to[3];
   double mr;
   
   FOR (i,n*n) Q[i]=0;
   for (i=0; i<n; i++) FOR (j,i) {
      from[0]=i/16; from[1]=(i/4)%4; from[2]=i%4;
      to[0]=j/16;   to[1]=(j/4)%4;   to[2]=j%4;
      c[0]=GeneticCode[com.icode][i];   c[1]=GeneticCode[com.icode][j];
      if (c[0]==-1 || c[1]==-1)  continue;
      for (k=0,ndiff=0; k<3; k++)  if (from[k]!=to[k]) { ndiff++; pos=k; }
      if (ndiff!=1)  continue;
      Q[i*n+j]=1;
      if ((from[pos]+to[pos]-1)*(from[pos]+to[pos]-5)==0)  Q[i*n+j]*=kappa;
      if(c[0]!=c[1])  Q[i*n+j]*=omega;
      Q[j*n+i]=Q[i*n+j];
   }
   FOR(i,n) FOR(j,n) Q[i*n+j]*=com.pi[j];
   for(i=0,mr=0;i<n;i++) { 
      Q[i*n+i]=-sum(Q+i*n,n); mr-=pi[i]*Q[i*n+i]; 
   }

   if(getstats)
      Qfactor+=freqK_NS *mr;
   else {
      if(com.ncatG==0) FOR(i,n*n) Q[i]*=1/mr;
      else             FOR(i,n*n) Q[i]*=Qfactor;  /* NSsites models */
      eigenQREV(Q, com.pi, com.pi_sqrt, n, com.npi0, Root, U, V);
   }

   return (0);
}



int EigenQaa(double pi[],double Root[], double U[], double V[],double Q[])
{
/* Construct the rate matrix Q[]
*/
   int n=20, i,j;
   double mr;

   FOR (i,n*n) Q[i]=0;
   switch (com.model) {
   case (Poisson)   : case (EqualInput) : 
      fillxc (Q, 1., n*n);  break;
   case (Empirical)   : case (Empirical_F):
      FOR(i,n) FOR(j,i) Q[i*n+j]=Q[j*n+i]=com.daa[i*n+j]/100;
      break;
   }
   FOR (i,n) FOR (j,n) Q[i*n+j]*=com.pi[j];
   for (i=0,mr=0; i<n; i++) {
      Q[i*n+i]=0; Q[i*n+i]=-sum(Q+i*n,n);  mr-=com.pi[i]*Q[i*n+i]; 
   }

   eigenQREV(Q, com.pi, com.pi_sqrt, n, com.npi0, Root, U, V);
   FOR(i,n)  Root[i]=Root[i]/mr;

   return (0);
}


int GetDaa (FILE* fout, double daa[])
{
/* Get the amino acid substitution rate matrix (grantham, dayhoff, jones, etc).
*/
   FILE * fdaa;
   char aa3[4]="";
   int i,j, n=20;

   if ((fdaa=fopen(com.daafile, "r"))==NULL) 
      { printf("\nAA dist file %s not found.", com.daafile); exit(-1); }
   printf("\nReading rate matrix from %s\n", com.daafile);

   for (i=0; i<n; i++)  for (j=0,daa[i*n+i]=0; j<i; j++)  {
      fscanf(fdaa, "%lf", &daa[i*n+j]);
      daa[j*n+i]=daa[i*n+j];
   }
   if (com.model==Empirical) {
      FOR(i,n) if(fscanf(fdaa,"%lf",&com.pi[i])!=1) error2("err aaRatefile");
      if (fabs(1-sum(com.pi,20))>1e-4) error2("\nSum of aa freq. != 1\n");
   }
   fclose (fdaa);

   if (fout) {
      fprintf (fout, "\n%s\n", com.daafile);
      FOR (i,n) {
         fprintf (fout, "\n%4s", getAAstr(aa3,i));
         FOR (j,i)  fprintf (fout, "%5.0f", daa[i*n+j]); 
      }
      FPN (fout);
   }

   return (0);
}




void MakeSeq(char*z, int ls)
{
/* makes a base, amino acid, or codon sequence using com.pi[].  
*/
   int j,h, n=com.ncode;
   double p[64],r, small=4e-6;

   FOR(j,n) p[j]=com.pi[j];
   for (j=1; j<n; j++) p[j]+=p[j-1];
   if (fabs(p[n-1]-1) > small)
      { printf("\nsum pi = %.6f != 1!\n",p[n-1]); exit(-1); }
   for (h=0; h<com.ls; h++) {
      for(j=0,r=rndu();j<n-1;j++) if(r<p[j]) break;
      z[h]=(char)j;
   }
}

void Evolve1 (int inode)
{
/* evolve sequence com.z[tree.root] along the tree to generate com.z[], 
   using nodes[].branch, nodes[].omega, & com.model
   Needs com.z[0,1,...,nnode-1], while com.z[0] -- com.z[ns-1] constitute
   the data.
   For codon sequences, com.rates[] has w's for NSsites models
*/
   int is, h,i,j, ison, from, n=com.ncode;
   double t, r;
   
   for (is=0; is<nodes[inode].nson; is++) {
      ison=nodes[inode].sons[is];
      memcpy(com.z[ison],com.z[inode],com.ls*sizeof(char));
      t=nodes[ison].branch;

      FOR (h,(t>1e-20)*com.ls) {
         /* decide whether to update PMat[] */
         if (h==0 || (com.rates && com.rates[h]!=com.rates[h-1])) {
            r=(com.rates?com.rates[h]:1);
            switch(com.seqtype) {
	         case (BASEseq):
               if(com.model<=TN93)
                  PMatTN93(PMat, t*Qfactor*r*Qrates[0], 
                                 t*Qfactor*r*Qrates[1], t*Qfactor*r, com.pi);
               else if(com.model==REV)
                  PMatUVRoot(PMat, t*r, com.ncode, U,V,Root);
	            break;

	         case (CODONseq): /* Watch out for NSsites model */
               if(com.model) r=nodes[ison].omega;
               if(com.rates || com.model)
                  EigenQcodon(0, com.kappa,r,com.pi, Root,U,V, PMat);
               PMatUVRoot(PMat,t,com.ncode,U,V,Root); 
               break;

	         case (AAseq):
               PMatUVRoot(PMat, t*r, com.ncode, U,V,Root);
               break;
            }
            FOR(i,n) for(j=1;j<n;j++)  PMat[i*n+j]+=PMat[i*n+j-1];
         }
         for(j=0,from=com.z[ison][h],r=rndu(); j<n-1; j++)
            if(r<PMat[from*n+j]) break;
         com.z[ison][h]=j;
      }
      if (nodes[ison].nson) Evolve1(ison); 
   }  /* for (is) */
}


void Simulate (char*ctlf)
{
/* simulate nr data sets of nucleotide, codon, or AA sequences.
   ls: number of sites (codons for codon sequences)
   All 64 codons are used for codon sequences.
   When com.alpha or com.ncatG>1, sites are randomized after sequences are 
   generated.
   space[com.ls] is used to hold site marks.
*/
   int verbose=1;
   char *ancf="ancestral.seq", *sitesf="siterates";
   enum {PAML,PAUP};
   FILE *fin, *fseq, *ftree=NULL, *fanc=NULL, *fsites=NULL;
   char *paupstart="paupstart",*paupblock="paupblock",*paupend="paupend";
   char *siterate=NULL;  /* used if ncatG>1 */
   int i,j,k, ir,n,nr,fixtree=1,sspace=10000,rooted=0;
   int h=0,format=PAML,b[3]={0}, nrate=1, counts[NCATG];
   int *siteorder=NULL;
   char *tmpseq=NULL;
   double birth=0, death=0, sample=1, mut=1, tlength, *space;
   double T,C,A,G,Y,R;

   com.alpha=0; com.cleandata=1; com.model=0;

   printf("\nReading options from data file %s\n", ctlf);
   com.ncode=n=(com.seqtype==0 ? 4 : (com.seqtype==1?64:20));
   if((fin=(FILE*)fopen(ctlf,"r"))==NULL) 
      { printf("\ndata file %s does not exist?\n", ctlf);  exit(-1); }
   fscanf (fin, "%d", &format);
   printf("\nSimulated data will go into %s.\n", seqf[format]);
   if(format==PAUP) printf("%s, %s, & %s will be appended if existent.\n",
                       paupstart,paupblock,paupend);

   fscanf (fin, "%d%d%d%d", &i,&com.ns, &com.ls, &nr);
   SetSeed(i);
   i=(com.ns*2-1)*sizeof(struct TREEN);
   if((nodes=(struct TREEN*)malloc(i))==NULL) error2("oom");

   if(com.ns>NS) error2("too many seqs?");
   printf ("\n%d seqs, %d sites, %d replicates\n", com.ns,com.ls,nr);
   k=(com.ns*com.ls* (com.seqtype==CODONseq?4:1) *nr)/1000+1;
   printf ("Seq file will be about %dK bytes.\n",k);

   fscanf(fin,"%lf",&tlength);
   if (fixtree) {
      if(ReadaTreeN(fin,&i,1))  error2("err tree..");
      if(!i) {  /* if tree does not have branch lengths */
         FOR(i,tree.nbranch)
            fscanf(fin,"%lf",&nodes[tree.branches[i][1]].branch);
      }
      if(tlength>0) {
         for(i=0,T=0; i<tree.nnode; i++) 
            if(i!=tree.root) T+=nodes[i].branch;
         for(i=0; i<tree.nnode; i++) 
            if(i!=tree.root) nodes[i].branch*=tlength/T;
      }
      if(com.ns<100) {
         printf("\nModel tree & branch lengths:\n"); OutaTreeN(F0,0,1); FPN(F0);
      }
      if(com.ns<30) { 
         OutaTreeB(F0); FPN(F0);
         FOR(i,tree.nbranch) printf("%9.5f",nodes[tree.branches[i][1]].branch);
         FPN(F0);
      }
      if(com.seqtype==CODONseq) {
#ifdef CodonNSbranches
         com.model=0;
         FOR(i,tree.nbranch)
            fscanf(fin,"%lf",&nodes[tree.branches[i][1]].omega);
         printf("and dN/dS ratios:\n"); 
         FOR(i,tree.nbranch) printf("%9.5f",nodes[tree.branches[i][1]].omega);
         FPN(F0);
#endif
      }
   }
   else {   /* random trees */
      printf ("\nbirth rate, death rate, sampling fraction, and ");
      printf ("mutation rate (tree height)?\n");
      fscanf (fin, "%lf%lf%lf%lf", &birth, &death, &sample, &mut);
   }

   if(com.seqtype==BASEseq) {
      fscanf(fin,"%d",&com.model);
      if(com.model<0 || com.model>REV) error2("model err");
      printf("\nModel: %s\n", basemodels[com.model]);
      if(com.model==REV)        nrate=5;
      else if(com.model==TN93)  nrate=2;
      FOR(i,nrate) fscanf(fin,"%lf",&Qrates[i]);
      if(nrate<=2) FOR(i,nrate) printf("kappa %9.5f\n",Qrates[i]); FPN(F0);
      if(nrate==5) {
         printf("a & b & c & d & e: ");
         FOR(i,nrate) printf("%9.5f",Qrates[i]); FPN(F0);
      }
      if((com.model==JC69 || com.model==F81)&&Qrates[0]!=1) 
         error2("kappa should be 1 for this model");
   }
   else if(com.seqtype==CODONseq) {
#ifdef  CodonNSsites
      fscanf(fin,"%d",&com.ncatG);
      if(com.ncatG>NCATG) error2("ncatG>NCATG");
      FOR(i,com.ncatG) fscanf(fin,"%lf",&com.freqK[i]);
      FOR(i,com.ncatG) fscanf(fin,"%lf",&com.rK[i]);
      printf("\n\ndN/dS for site classes (K=%d)\np: ",com.ncatG);
      FOR(i,com.ncatG) printf("%9.5f",com.freqK[i]);
      printf("\nw: ");
      FOR(i,com.ncatG) printf("%9.5f",com.rK[i]);  FPN(F0);
      verbose=1;
#else
#ifndef CodonNSbranches
      fscanf(fin,"%lf",&com.omega);
      printf("omega = %9.5f\n",com.omega);
      FOR(i,tree.nbranch) nodes[tree.branches[i][1]].omega=com.omega;
#endif
#endif
      fscanf(fin,"%lf",&com.kappa);
      printf("kappa = %9.5f\n",com.kappa);
   }

   if(com.seqtype==BASEseq || com.seqtype==AAseq) {
      fscanf(fin,"%lf%d", &com.alpha, &com.ncatG);
      if(com.alpha) 
        printf("Gamma rates, alpha =%.4f (K=%d)\n",com.alpha,com.ncatG);
      else { com.ncatG=0; puts("Rates are constant over sites."); }
   }
   if(com.alpha || com.ncatG) { /* this is used for codon NSsites as well. */
      if((com.rates=(double*)malloc(com.ls*sizeof(double)))==NULL) error2("oom1");
      if((siteorder=(int*)malloc(com.ls*sizeof(int)))==NULL) error2("oom2");
   }

   /* get aa substitution model and rate matrix */
   if(com.seqtype==AAseq) {
      fscanf(fin,"%d",&com.model);
      printf("\nmodel: %s",aamodels[com.model]); 
      if(com.model>=2)  { fscanf(fin,"%s",com.daafile); GetDaa(NULL,com.daa); }
   }
   /* get freqs com.pi[] */
   if((com.seqtype==BASEseq && com.model>K80) ||
       com.seqtype==CODONseq ||
      (com.seqtype==AAseq && (com.model==1 || com.model==3)))
         FOR(k,com.ncode) fscanf(fin,"%lf",&com.pi[k]);
   else if(com.model==0 || (com.seqtype==BASEseq && com.model<=K80)) 
      fillxc(com.pi,1./com.ncode,com.ncode);
   for(j=0,com.npi0=0; j<com.ncode; j++)
      if(com.pi[j]) com.pi_sqrt[com.npi0++]=sqrt(com.pi[j]);
   com.npi0=com.ncode-com.npi0;

   printf("sum pi = 1 = %.6f:", sum(com.pi,com.ncode));
   matout2(F0,com.pi,1,com.ncode,7,4);
   if(com.seqtype==BASEseq) {
      if(com.model<REV) {
         T=com.pi[0]; C=com.pi[1]; A=com.pi[2]; G=com.pi[3]; Y=T+C; R=A+G;
         if (com.model==F84) { 
            Qrates[1]=1+Qrates[0]/R;   /* kappa2 */
            Qrates[0]=1+Qrates[0]/Y;   /* kappa1 */
         }
         else if (com.model<=HKY85) Qrates[1]=Qrates[0];
         Qfactor=1/(2*T*C*Qrates[0] + 2*A*G*Qrates[1] + 2*Y*R);
      }
      else
         if(com.model==REV) EigenQbase(Qrates, com.pi, Root,U,V,PMat);
   }

   /* get Qfactor for NSsites models */
   if(com.seqtype==CODONseq && com.ncatG) {
      for(j=0,Qfactor=0; j<com.ncatG; j++) {
         freqK_NS=com.freqK[j];
         EigenQcodon(1, com.kappa,com.rK[j],com.pi, NULL,NULL,NULL, PMat);
      }
      Qfactor=1/Qfactor;
      printf("Qfactor = %9.5f\n", Qfactor);
   }
   if(com.seqtype==CODONseq && com.ncatG<=1 && com.model==0)
      EigenQcodon(0, com.kappa,com.omega,com.pi, Root,U,V, PMat);
   else if(com.seqtype==AAseq)
      EigenQaa(com.pi,Root, U, V,PMat);

   puts("\nAll parameters are read, now ready to simulate\n");
   FOR(i,com.ns)sprintf(com.spname[i],"seq.%d",i+1);
   FOR(j,com.ns*2-1) com.z[j]=(char*)malloc(com.ls*sizeof(char));
   sspace=max2(sspace, com.ls*(int)sizeof(double));
   space=(double*)malloc(sspace);
   if(com.alpha || com.ncatG) tmpseq=(char*)space;
   if (com.z[com.ns*2-1-1]==NULL || space==NULL) error2("oom for space");
   if((fseq=fopen(seqf[format],"w"))==NULL) error2("seq file creation error.");
   if(format==PAUP) appendfile(fseq,paupstart);
   if(verbose) {
      if((fanc=(FILE*)fopen(ancf,"w"))==NULL) error2("anc file creation error2");
      fputs("\nAncestral sequences generated during simulation ",fanc);
      fprintf(fanc,"(check against %s)\n",seqf[format]);
      OutaTreeN(fanc,0,0); FPN(fanc); OutaTreeB(fanc); FPN(fanc);

      if(com.alpha || com.ncatG>1) {
         if((fsites=(FILE*)fopen(sitesf,"w"))==NULL) error2("sitesf creation error");
         if(com.seqtype==1) fputs("\nList of sites for last omega\nomega's",fsites);
         else               fputs("\nRates for sites",fsites);
         if(com.seqtype==CODONseq && com.ncatG>1) {
            matout(fsites,com.rK, 1,com.ncatG);
            if((siterate=(char*)malloc(com.ls*sizeof(char)))==NULL) 
               error2("oom siterate");
         }
      }
   }

   for (ir=0; ir<nr; ir++) {
      if (!fixtree) {    /* right now tree is fixed */
         RandomLHistory (rooted, space);
         if (rooted && com.ns<10) j=GetIofLHistory ();
         BranchLengthBD (1, birth, death, sample, mut);
         if (rooted && com.ns<10) printf ("\ntree used (LH #%d):\n", j);
         else                     printf ("\ntree used: "); 
         OutaTreeN(F0,0,1);  puts(";");
      }
      MakeSeq(com.z[tree.root],com.ls);

      if (com.alpha)
         Rates4Sites (com.rates,com.alpha,com.ncatG,com.ls, 0,space);
      else if(com.seqtype==1 && com.ncatG) { /* for NSsites */
         MultiNomial (com.ls, com.ncatG, com.freqK, counts, space);
         for (i=0,h=0; i<com.ncatG; i++)
            for (j=0; j<counts[i]; j++) {
               if(verbose) siterate[h]=i;
               com.rates[h++]=com.rK[i];
            }
      }

      Evolve1(tree.root);

      /* randomize sites for site-class model */
      if(com.rates && com.ncatG>1) {
         randorder(siteorder, com.ls, (int*)space);
         FOR(j,tree.nnode) {
            memcpy(tmpseq,com.z[j],com.ls*sizeof(char));
            FOR(h,com.ls) com.z[j][h]=tmpseq[siteorder[h]];
         }
         if(com.alpha || com.ncatG>1) {
            memcpy(space,com.rates,com.ls*sizeof(double));
            FOR(h,com.ls) com.rates[h]=space[siteorder[h]];
         }
         if(siterate) {
            memcpy((char*)space,siterate,com.ls*sizeof(char));
            FOR(h,com.ls) siterate[h]=*((char*)space+siteorder[h]);
         }
      }

      /* print ancestral seqs, rates for sites */
      if(verbose) {
         j=(com.seqtype==CODONseq?3*com.ls:com.ls);
         fprintf(fanc,"\n[replicate %d]\n",ir+1);
         fprintf(fanc,"%6d %6d\n",tree.nnode-com.ns,j);
         for(j=com.ns; j<tree.nnode; j++,FPN(fanc)) {
            fprintf(fanc,"node%-26d  ", j+1);
            print1seq(fanc,com.z[j],com.ls,1, NULL);
         }
         if(fsites) {
            if(com.seqtype==CODONseq && com.ncatG>1) {
               for(h=0,j=0; h<com.ls; h++)  if(siterate[h]==com.ncatG-1) j++;
               fprintf(fsites,"\n[replicate %d: %2d]\n",ir+1,j);
               FOR(h,com.ls)
                  { if(siterate[h]==com.ncatG-1) fprintf(fsites,"%4d ",h+1);}
            }
            else {
               fprintf(fsites,"\n[replicate %d]\n",ir+1);
               FOR(h,com.ls) {
                  fprintf(fsites,"%7.4f ",com.rates[h]);
                  if((h+1)%10==0) FPN(fsites);
               }
            }
            FPN(fsites);
         }
      }

      /* print sequences*/
      if (format==PAUP) fprintf(fseq,"\n\n[Replicate # %d]\n", ir+1);
      printSeqs(fseq, NULL, NULL, format);
      if(format==PAUP && !fixtree) {
         fprintf(fseq,"\nbegin tree;\n   tree true_tree = [&U] "); 
         OutaTreeN(fseq,1,1); fputs(";\n",fseq);
         fprintf(fseq,"end;\n\n");
      }
      if(format==PAUP) appendfile(fseq,paupblock);

      printf ("\rdid data set %d.", ir+1);
   }   /* for (ir) */

   if(format==PAUP) appendfile(fseq,paupend);

   fclose(fseq);  if(fanc) fclose(fanc);  if(fsites) fclose(fsites);
   FOR(j,com.ns*2-1) free(com.z[j]);
   free(space);  free(nodes);
   if(com.alpha || com.ncatG) { 
      free(com.rates);  free(siteorder);
      if(siterate) free(siterate);
   }
   exit (0);
}
