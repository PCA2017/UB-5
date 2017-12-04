//============================================================================
// Name        : heated_plate_pthreads.cpp
// Author      : pra01
// Description : Calculates the heat distribution on a plate iteratively.
//               Boundary has the const value 0.0.
//               The initial Matrix can have a circle of a specified Temperature
//               between 0.0 and 127.0 in the middle.
//				 The calculation for each timestep is done in parallel with a specified
//               number of Pthreads.
// Arguments   : n - Dimension of the quadratic discrete heated plate
//               r - Radius of an inital circle with specified Temperature
//               H - Temperature of the initial circle between 0.0 and 127.0
//               num_threads - Number of pthreads
//               filename - Name of the file whitch will contain the simulation results
// Example     : ./heated_plate 10 4 10.0 5 file_1
//               Creates a 10x10 plate with a initial circle of radius 4 with
//               Temperature 10.0 with 5 PThreads calculating each step
//               and saves the results in an File with name file_1
//============================================================================

#include <iostream>
#include <fstream>
#include <new>
#include <sys/time.h>
#include <pthread.h>

using namespace std;

void init_matrix(double ** matrix, int n, double a, double b, int r, double H);   // Initiale Matrix erzeugen
void* calculate_slice(void* inf); // Updatet den Wert in matrix anhand Wärmeleitungsgleichung und der alten Matrix
void ergebniszeile_eintragen(double ** matrix, int n, ofstream &);   // Ergebnis der Matrix im aktuellen Zeitrschitt in Datei eintragen

struct Info
{
	int n;
	const double phi;
	double ** matrix;
	double ** matrix_old;
	int num_threads;
	int t_id;
};

int main(int argc, char **argv) {

	unsigned int i, n, r, t, num_threads;  // Nur Werte > 0 möglich
	double H, a, b;

	const double phi = 6.0/25.0;

	double ** m1;
	double ** m1old; // Matrix aus dem Zeitschritt davor

	struct timeval start, stop;   // Zeitmessung mit gettimeofday
	long seconds, useconds;
	double duration;

	string filename;
	ofstream ergebnisdatei;

	n = atoi(argv[1]);
	r = atoi(argv[2]);
	H = atoi(argv[3]);
	num_threads = atoi(argv[4]);
	filename = argv[5];

	ergebnisdatei.open(filename.c_str(),ios::out);

	// H auf Bereich 0.0 bis 127.0 begrenzen. (Werte in der Matrixen können dann Aufgrund der Berechnungsvorschrift den Wertebereich auch nicht verlassen.)
	if (H > 127.0)
		H = 127.0;
	else if (H < 0.0)
		H = 0.0;

	// Radius des erhitzten Kreises darf nicht den Rand der Matrix berühren
	if (r > ((n/2) -2))
		r = ((n/2) -2);

	a = (n-1)/2.0;  // Mittelpunkt Kreis in x-Richtung
	b = (n-1)/2.0;  // Mittelpunkt Kreis in y-Richtung

	// Matrix mit (nxn) Einträgen
	m1 = new double *[n];
	m1[0] = new double [n*n];
	for(i = 1; i<n; i++)
		m1[i] = m1[0] + i*n;

	// Matrix aus dem letzten Zeitschritt mit (nxn) Einträgen
	m1old = new double *[n];
	m1old[0] = new double [n*n];
	for(i = 1; i<n; i++)
		m1old[i] = m1old[0] + i*n;

	if (m1 == nullptr || m1old == nullptr)
		cout << "Error: memory could not be allocated";
	else
	{
		// Matrix initialisieren
		init_matrix(m1, n, a, b, r, H);

		//ergebniszeile_eintragen(m1, n, ergebnisdatei);  // Initiale Matrix in Datei schreiben

		// Threads erzeugen
		pthread_t pThreads [num_threads];


		// Zeit stoppen
		gettimeofday(&start, NULL);

		// Innere Werte der Matrix (ohne Randwerte) iterativ updaten
		for (t = 0; t<100; t++)  // 100 Zeitschritte
		{
			// Werte aus dem letzten Zeitschritt in m1old zwischenspeichern
				for (int i=0; i<n; i++)
				{
					for (int j=0; j<n; j++)
						m1old[i][j] = m1[i][j];
				}

				// Parallel Werte berechnen

				for(int i = 0; i < num_threads; ++i)
				{
					//cout << "Arbeitszuweisung an Threads" << endl;
					Info* info = new Info{n, phi, m1, m1old, num_threads, i+1};
					pthread_create(&pThreads[i], NULL, &calculate_slice, (void*) info);
				}

				// Wait for threads to finish
				for(int i = 0; i < num_threads; ++i)
				{
					pthread_join(pThreads[i], NULL);
				}

			//ergebniszeile_eintragen(m1, n, ergebnisdatei);
		}

		// Zeit stoppen
		gettimeofday(&stop, NULL);

		// Dauer berechnen
		seconds  = stop.tv_sec  - start.tv_sec;
		useconds = stop.tv_usec - start.tv_usec;
		duration = seconds + useconds/1000000.0;  // Dauer in Sekunden

		cout << "Dauer: " << duration << " Sekunden" << endl;

		delete[] m1old[0];
		delete[] m1old;
		delete[] m1[0];
		delete[] m1;
	}

	return 0;
}

void init_matrix(double ** matrix, int n, double a, double b, int r, double H){
// Initialisiert eine (nxn) Matrix mit 0en bis auf einen Kreis mit den Werten H mit Radius r und Mittelpunkt bei (a,b)

	unsigned int r2 = r*r; // r^2

	for (int i=0; i<n; i++)
	{
		for (int j=0; j<n; j++)
		{
			// Punkte im Kreis zu H setzten
			if (((i-a)*(i-a)+(j-b)*(j-b)) < r2)
				matrix[i][j] = H;
			// Rest zu =0.0 setzten
			else
				matrix[i][j] = 0.0;  // Achtung: Wenn Kreisradius zu groß übergeben wurden, Randwerte !=0 gesetzt die sich nicht verändern! -> Bei Eingabe geprüft
		}
	}
}

void* calculate_slice(void* inf){
// Updatet den Wert in matrix anhand Wärmeleitungsgleichung mit phi und der alten Matrix matrix_old
// Soll von parallel arbeitenden Threads ausgeführt werden.

	Info* info = (Info*) inf;
	int n = info->n;
	const double phi = info->phi;
	double ** matrix = info->matrix;
	double ** matrix_old = info->matrix_old;
	int num_threads = info->num_threads;
	int thread_id = info->t_id;

	int intervall = n/num_threads;
	int i_end = thread_id * intervall;
	int i_start = i_end - intervall;

	if (i_start < 1)
		i_start = 1;  // Randwert bei i=1 nicht ändern

	if (i_end >= n)
		i_end = n-1; // Randwert bei i=n nicht ändern

	//cout << "Thread ID: " << thread_id << endl;

	// Werte des aktuellen Zeitschritts berechnen
	for (int i=i_start; i<i_end; i++)
	{
		for (int j=1; j<(n-1); j++)
			matrix[i][j] = matrix_old[i][j] + phi*((-4)*matrix_old[i][j] + matrix_old[i+1][j] + matrix_old[i-1][j] + matrix_old[i][j+1] + matrix_old[i][j-1]);
	}
}

void ergebniszeile_eintragen(double ** matrix, int n, ofstream &OUT){
// Ergebniss eines Zeitschritts in OUT schreiben.

	for (int i=0; i<n; i++)
	{
		for (int j=0; j<n; j++)
			OUT << matrix[i][j] << ",";

		OUT << endl;
	}
	OUT << "#" << endl;
}
