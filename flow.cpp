#include<iostream>
#include<unordered_map>
#include<unordered_set>
#include<vector>
#include<list>

using namespace std;

int main(int argc, char* argv[]){
    cout<<"Hey! Welcome to this program \n";
    for(int i=0;i<argc;i++){
        cout<<argv[i]<<endl;
    }
    return 0;
}