/* Full C++17 port of the SLALOM radiosity benchmark stages. */
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>
#include "file.hpp"
#include "system_info.h"

namespace {
constexpr int NMAX = 8192;
constexpr double EPS = 0.5e-8;
using Vec = std::vector<double>;
struct Matrix {
    explicit Matrix(int n = 0) : n(n), v(static_cast<std::size_t>(n) * n, 0.0) {}
    double &operator()(int r, int c) { return v[static_cast<std::size_t>(r) * n + c]; }
    double operator()(int r, int c) const { return v[static_cast<std::size_t>(r) * n + c]; }
    int n; Vec v;
};
using FaceRGB = std::array<std::array<double, 3>, 6>;
struct Geometry { std::array<double, 7> box{}; FaceRGB rho{}, emiss{}; };
struct Regions {
    std::array<std::array<int, 2>, 6> loop{};
    std::array<Vec, 3> place; std::array<Vec, 2> size; Vec area;
    explicit Regions(int n) : area(n) { for (auto &x: place) x.resize(n); for (auto &x: size) x.resize(n); }
};
const char *safe(const char *s) { return s ? s : "unknown"; }

// Four-way unrolled, strided double dot product, retaining the reference API's
// element-stride semantics and a scalar cleanup for every vector length.
double DDot(int n, const double *a, int ia, const double *b, int ib) {
    double s0=0, s1=0, s2=0, s3=0;
    int i=0;
    for (; i+3<n; i+=4) {
        s0 += a[i*ia]*b[i*ib]; s1 += a[(i+1)*ia]*b[(i+1)*ib];
        s2 += a[(i+2)*ia]*b[(i+2)*ib]; s3 += a[(i+3)*ia]*b[(i+3)*ib];
    }
    double sum=s0+s1+s2+s3;
    for (; i<n; ++i) sum += a[i*ia]*b[i*ib];
    return sum;
}

Geometry Reader() {
    slalom::File file("geom", "r");
    if (!file) throw std::runtime_error("'geom' not found: " + file.path());
    Geometry g;
    int got=0;
    for(int i=0;i<3;i++) got += std::fscanf(file.get(), "%lg", &g.box[i]);
    char line[81]; (void)std::fgets(line, sizeof(line), file.get());
    if(got!=3) throw std::runtime_error("Must specify exactly 3 box coordinates.");
    got=0;
    for(int c=0;c<3;c++) for(int f=0;f<6;f++) got+=std::fscanf(file.get(), "%lg", &g.rho[f][c]);
    (void)std::fgets(line, sizeof(line), file.get());
    if(got!=18) throw std::runtime_error("Must specify exactly 18 reflectivities.");
    got=0;
    for(int c=0;c<3;c++) for(int f=0;f<6;f++) got+=std::fscanf(file.get(), "%lg", &g.emiss[f][c]);
    if(got!=18) throw std::runtime_error("Must specify exactly 18 emissivities.");
    for(int j=0;j<3;j++) {
        if(g.box[j]<1.0 || g.box[j]>=100.0) throw std::runtime_error("Box dimensions must be between 1 and 100.");
        g.box[j+3]=g.box[j]; double max_emiss=0;
        for(int f=0;f<6;f++) {
            if(g.rho[f][j]<0 || g.rho[f][j]>0.999) throw std::runtime_error("Reflectivities must be between .000 and .999.");
            if(g.emiss[f][j]<0) throw std::runtime_error("Emissivity cannot be negative.");
            max_emiss=std::max(max_emiss,g.emiss[f][j]);
        }
        if(max_emiss==0) throw std::runtime_error("Emissivities are zero. Problem is trivial.");
    }
    g.box[6]=g.box[3]; return g;
}

bool Region(int n, const Geometry &g, Regions &r) {
    const auto &box=g.box;
    double total=2.0*(box[0]*box[1]+box[1]*box[2]+box[2]*box[0]);
    double accum=0, remain=n; r.loop[0][0]=0;
    for(int f=0;f<5;f++) { accum+=box[f]*box[f+1]; r.loop[f][1]=static_cast<int>(remain*accum/total+0.5)-1; r.loop[f+1][0]=r.loop[f][1]+1; }
    r.loop[5][1]=n-1;
    for(int f=0;f<6;f++) {
        int count=r.loop[f][1]-r.loop[f][0]+1; double depth=f>=3?box[f-1]:0;
        int cols=static_cast<int>(std::sqrt(count*box[f]/box[f+1])+0.5);
        cols=std::max(1,std::min(cols,count)); double width=box[f]/cols; int itmp=cols-1; double x=0;
        for(int col=0;col<cols;col++) {
            int q=itmp/cols, rows=(itmp+count)/cols-q;
            if(rows==0) { std::cerr<<"Eccentric box requires more patches.\n"; return false; }
            double height=box[f+1]/rows, y=0, area=width*height*(8.0*M_PI);
            for(int p=r.loop[f][0]+q;p<r.loop[f][0]+q+rows;p++) {
                r.size[0][p]=width; r.size[1][p]=height;
                r.place[0][p]=x; r.place[1][p]=y; r.place[2][p]=depth; r.area[p]=area; y+=height;
            }
            x+=width; itmp+=count;
        }
    }
    return true;
}

void SetUp1(int n, const Regions &r, Matrix &a) {
    double d[2][2][2], d2[2][2][2];
    for(int f=0;f<3;f++) { int jf=f+3; double plane=r.place[2][r.loop[jf][0]]*r.place[2][r.loop[jf][0]];
        for(int p=r.loop[f][0];p<=r.loop[f][1];p++) for(int q=r.loop[jf][0];q<=r.loop[jf][1];q++) {
            for(int j=0;j<2;j++) {
                d[0][0][j]=r.place[j][q]-r.place[j][p]; d[1][0][j]=d[0][0][j]+r.size[j][q];
                d[0][1][j]=d[0][0][j]-r.size[j][p]; d[1][1][j]=d[1][0][j]-r.size[j][p];
                for(int m=0;m<2;m++) for(int i=0;i<2;i++) d2[m][i][j]=d[m][i][j]*d[m][i][j];
            }
            double t2=0;
            for(int j=0;j<2;j++) for(int m=0;m<2;m++) for(int i=0;i<2;i++) {
                double t3=d2[m][i][1-j]+plane,t4=std::sqrt(t3),t5=1.0/t4,t8=0;
                for(int k=0;k<2;k++) { for(int h=0;h<2;h++) { double t6=d2[k][h][j],t7=d[k][h][j]; t2=(t3-t6)*std::log(t6+t3)-t2; t8=-t7*std::atan(t7*t5)-t8; } t2=-t2; t8=-t8; }
                t2=-4.0*t4*t8-t2;
            }
            a(p,q)=a(q,p)=t2;
        }
    }
}
void SetUp2(int n, const Regions &r, Matrix &a) {
    double x[2][2], x2[2][2], y[2][2];
    for(int f=0;f<6;f++) for(int m=0;m<2;m++) { int jf=(f+m+1)%6;
        for(int p=r.loop[f][0];p<=r.loop[f][1];p++) for(int q=r.loop[jf][0];q<=r.loop[jf][1];q++) {
            x[0][0]=r.place[m][q]-r.place[1-m][p]; x[1][0]=x[0][0]+r.size[m][q];
            x[0][1]=x[0][0]-r.size[1-m][p]; x[1][1]=x[1][0]-r.size[1-m][p];
            y[0][0]=r.place[m][p]-r.place[2][q]; y[1][0]=y[0][0]+r.size[m][p];
            y[0][1]=r.place[1-m][q]-r.place[2][p]; y[1][1]=y[0][1]+r.size[1-m][q];
            for(int i=0;i<2;i++) for(int j=0;j<2;j++) { y[i][j]*=y[i][j]; x2[i][j]=x[i][j]*x[i][j]; }
            for(int i=0;i<2;i++) y[i][1]+=1e-35;
            double t2=0;
            for(int i=0;i<2;i++) for(int j=0;j<2;j++) {
                double t3=y[i][0]+y[j][1],t4=std::sqrt(t3),t5=1.0/t4,t8=0;
                for(int k=0;k<2;k++) { for(int h=0;h<2;h++) { double t6=x2[k][h],t7=x[k][h]; t2=(t3-t6)*std::log(t6+t3)-t2; t8=-t7*std::atan(t7*t5)-t8; } t2=-t2; t8=-t8; }
                t2=-4.0*t4*t8-t2;
            }
            a(p,q)=a(q,p)=std::fabs(t2);
        }
    }
}
bool SetUp3(int n, const Geometry &g, const Regions &r, Matrix &a, std::array<Vec,3> &diag, std::array<Vec,3> &rhs) {
    for(auto &v:diag) v.resize(n); for(auto &v:rhs) v.resize(n);
    for(int f=0;f<6;f++) for(int c=0;c<3;c++) {
        double inv=1.0/g.rho[f][c], em=g.emiss[f][c]*inv;
        for(int p=r.loop[f][0];p<=r.loop[f][1];p++) {
            double sum=0; for(int q=0;q<r.loop[f][0];q++) sum+=a(p,q);
            for(int q=r.loop[f][1]+1;q<n;q++) sum+=a(p,q);
            if(std::fabs(sum-r.area[p])>0.5e-9*sum) { std::cerr<<"Total form factor is too far from unity.\n"; return false; }
            sum=-sum; for(int q=r.loop[f][0];q<=r.loop[f][1];q++) a(p,q)=0;
            diag[c][p]=inv*sum; rhs[c][p]=em*sum;
        }
    }
    return true;
}
void Solver(int n, int non0, Matrix &a, const std::array<Vec,3> &diag, const std::array<Vec,3> &rhs, std::array<Vec,3> &result) {
    for(auto &v:result) v.resize(n);
    for(int c=0;c<3;c++) {
        for(int i=non0;i<n;i++) { a(i,i)=diag[c][i]; result[c][i]=rhs[c][i]; for(int j=0;j<i;j++) a(i,j)=a(j,i); }
        for(int j=0;j<non0;j++) { a(j,j)=1.0/diag[c][j]; result[c][j]=rhs[c][j]; }
        for(int j=non0;j<n;j++) {
            for(int k=non0;k<j;k++) a(j,k)-=DDot(k,a.v.data()+static_cast<std::size_t>(k)*n,1,a.v.data()+static_cast<std::size_t>(j)*n,1);
            for(int k=0;k<j;k++) { double t=a(j,k); a(j,k)=t*a(k,k); a(j,j)-=t*a(j,k); }
            a(j,j)=1.0/a(j,j);
        }
        for(int k=non0;k<n;k++) result[c][k]-=DDot(k,result[c].data(),1,a.v.data()+static_cast<std::size_t>(k)*n,1);
        for(int k=0;k<n;k++) result[c][k]*=a(k,k);
        for(int k=n-2;k>=non0;k--) result[c][k]-=DDot(n-k-1,result[c].data()+k+1,1,a.v.data()+static_cast<std::size_t>(k+1)*n+k,n);
        for(int k=non0-1;k>=0;k--) result[c][k]-=DDot(n-non0,result[c].data()+non0,1,a.v.data()+static_cast<std::size_t>(non0)*n+k,n);
    }
}
bool Verify(int n, Matrix &a, const std::array<Vec,3> &diag, const std::array<Vec,3> &rhs, const std::array<Vec,3> &x) {
    double err=0, anorm=0, xnorm=0;
    for(int c=0;c<3;c++) {
        for(int i=0;i<n;i++) { a(i,i)=diag[c][i]; for(int j=0;j<i;j++) a(i,j)=a(j,i); }
        anorm=xnorm=0;
        for(int j=0;j<n;j++) { double t=rhs[c][j]; for(int i=0;i<n;i++) { t-=a(j,i)*x[c][i]; anorm=std::max(anorm,std::fabs(a(j,i))); } xnorm=std::max(xnorm,std::fabs(x[c][j])); err+=std::fabs(t); }
    }
    err/=anorm*xnorm;
    if(err>3*EPS) { std::cerr<<"Residual is too large: "<<err<<'\n'; return false; }
    return true;
}
void Storer(int n, const Regions &r, const std::array<Vec,3> &x) {
    FILE *f=std::fopen("answer","w"); if(!f) throw std::runtime_error("Unable to open 'answer' file.");
    std::fprintf(f,"%d patches:\n Patch  Face       Position in w, h, d              Width     Height\n",n);
    for(int face=0;face<6;face++) for(int p=r.loop[face][0];p<=r.loop[face][1];p++)
        std::fprintf(f,"%5d   %4d%11.5lf%11.5lf%11.5lf  %11.5lf%11.5lf\n",p+1,face+1,r.place[0][p],r.place[1][p],r.place[2][p],r.size[0][p],r.size[1][p]);
    std::fprintf(f,"\n Patch  Face  Radiosities\n");
    for(int face=0;face<6;face++) for(int p=r.loop[face][0];p<=r.loop[face][1];p++)
        std::fprintf(f,"%5d   %4d%12.8lf%12.8lf%12.8lf\n",p+1,face+1,x[0][p],x[1][p],x[2][p]);
    std::fclose(f);
}
double now_seconds() { using C=std::chrono::steady_clock; return std::chrono::duration<double>(C::now().time_since_epoch()).count(); }
bool Meter(int n, double &elapsed, double &work) {
    if(n<6 || n>NMAX) { std::cerr<<"Problem size must be between 6 and "<<NMAX<<".\n"; return false; }
    (void)::unlink("answer"); double t0=now_seconds();
    Geometry g=Reader(); Regions r(n); if(!Region(n,g,r)) return false;
    Matrix a(n); SetUp1(n,r,a); SetUp2(n,r,a);
    std::array<Vec,3> diag,rhs,x; if(!SetUp3(n,g,r,a,diag,rhs)) return false;
    int non0=r.loop[1][0]; Solver(n,non0,a,diag,rhs,x); Storer(n,r,x);
    elapsed=now_seconds()-t0; if(elapsed==0) elapsed=1e-9;
    // Preserve the published benchmark's operation assessment.
    double p[6],sum=0,tmp=0; int itmp=0;
    for(int i=0;i<6;i++) { p[i]=r.loop[i][1]-r.loop[i][0]+1; tmp+=p[i]*p[i]; itmp+=static_cast<int>(std::sqrt(p[i]*g.box[i]/g.box[i+1])+0.5); }
    double cross=p[0]*p[3]+p[1]*p[4]+p[2]*p[5];
    double ops0=258,ops1=154+8.0*itmp+n,ops2=3+840*cross;
    double ops3=430*(static_cast<double>(n)*n-tmp-2*cross)/2;
    double ops4=72+9.0*n+static_cast<double>(n)*n-tmp;
    double q=non0+1.0,ops5=n*(n*(n+7.5)-2.5)-21+q*(q*(2*q-16.5)+35.5)+q*n*(9-3*q),ops6=48.0*n;
    work=ops0+ops1+ops2+ops3+ops4+ops5+ops6;
    std::cout<<n<<" patches, "<<elapsed<<" seconds, "<<(work/elapsed)*1e-6<<" MFLOPS\n";
    return Verify(n,a,diag,rhs,x);
}
int run(int argc,char **argv) {
    double goal=0; int lo=0,hi=0;
    for(int i=1;i<argc;i++) {
        if(i+1>=argc) throw std::runtime_error("Usage: slalom-cxx -t seconds -l lower -u upper");
        if(!std::strcmp(argv[i],"-t")||!std::strcmp(argv[i],"--time")) goal=std::atof(argv[++i]);
        else if(!std::strcmp(argv[i],"-l")||!std::strcmp(argv[i],"--lower-bound")) lo=std::atoi(argv[++i]);
        else if(!std::strcmp(argv[i],"-u")||!std::strcmp(argv[i],"--upper-bound")) hi=std::atoi(argv[++i]);
        else throw std::runtime_error("Unknown command-line option.");
    }
    if(goal<=0||lo<6||hi<=lo||hi>NMAX) throw std::runtime_error("Usage: slalom-cxx -t seconds -l lower(>=6) -u upper(<=8192)");
    double seconds,work;
    if(!Meter(lo,seconds,work)||seconds>=goal) throw std::runtime_error("Lower bound must take less than the requested time.");
    if(!Meter(hi,seconds,work)||seconds<goal) throw std::runtime_error("Upper bound must take at least the requested time.");
    while(hi-lo>1) { int mid=(lo+hi)/2; if(!Meter(mid,seconds,work)) return 1; if(seconds<goal) lo=mid; else hi=mid; std::cout<<"New interval: ["<<lo<<','<<hi<<"]\n"; }
    if(!Meter(lo,seconds,work)) return 1;
    const MachineInfo *info=get_system_info();
    std::cout<<"\nC++ SLALOM (std::vector), machine: "<<safe(info?info->arch:nullptr)<<", processor: "<<safe(info?info->proc:nullptr)<<"\n"
             <<"M ops: "<<work*1e-6<<"  Time: "<<seconds<<" seconds\nn: "<<lo<<"  MFLOPS: "<<(work/seconds)*1e-6<<"\n"
             <<"Approximate data memory use: "<<(8LL*lo*lo+120LL*lo+800)<<" bytes.\n";
    return 0;
}
}
int main(int argc,char **argv) {
    try { return run(argc,argv); }
    catch(const std::exception &e) { std::cerr<<"slalom-cxx: "<<e.what()<<'\n'; return 1; }
}
