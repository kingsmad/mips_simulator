//CDA 5155
//Project 1
//Text to binary convertor

#include<iostream>
#include<fstream>
#include<string>
#include<stdlib.h>

using namespace std;

int toInt(const char *str)
{		
	unsigned int i,val=0;
	
	for(i=0;i<32;i++)
	{
		val<<=1;
		val|=((int)str[i]-'0');
	}
	
	return val;
}

int little_endian2big_endian(unsigned int i)
{
    return((i&0xff)<<24)+((i&0xff00)<<8)+((i&0xff0000)>>8)+((i>>24)&0xff);
}

int main() {

    FILE* f = fopen("out.bin", "w");
    if (f == NULL) {
        cout << "\nCould not open output file ";
        return -1;
    }
    ifstream fin;
    fin.open("in.txt");
    if (!fin) {
        cout << "\nCould not open input file (in.txt) ";
        return -1;
    }
    string str;
    unsigned int val = 0;
    while (getline(fin, str)) {
        val = 0;
        const char *p = str.c_str();
        val=little_endian2big_endian(toInt(p));
        fwrite (&val, 4, 1, f );
    }
	
    cout<< "\nDone, out.bin is output file...\n";
    fclose(f);
    fin.close();
}
