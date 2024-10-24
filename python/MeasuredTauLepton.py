import numpy as np

electronMass = 0.51100e-3 #GeV
muonMass = 0.10566 #GeV
chargedPionMass = 0.13957 #GeV
neutralPionMass = 0.13498 #GeV
rhoMesonMass = 0.77526 #GeV
a1MesonMass = 1.230 #GeV
tauLeptonMass = 1.77685 #GeV
cTauLifetime = 8.711e-3 #centimeters
hbar_c = 0.1973 #GeV*fm
ctau = 87.e+9 # tau lifetime = 87 microns, converted to fm
GammaTau = hbar_c/ctau
GammaTauToElec = GammaTau*0.178 #BR(tau -> e) = 17.8%
GammaTauToMu = GammaTau*0.174 #BR(tau -> mu) = 17.4%
GammaTauToHad = GammaTau*0.648 #BR(tau -> hadrons) = 64.8%

#Normalization factors
conversionFactor = 1.e+10*np.square(hbar_c) # conversion factor from GeV^-2 to picobarn = 10^-40m
constFactor = 2.*conversionFactor/(2.*np.pi)**8
matrixElementNorm = np.square(np.pi/(tauLeptonMass*GammaTau)) # CV: multiply matrix element by factor (Pi/(mTau GammaTau))^2 from Luca's write-up

class MeasuredTauLepton:

    #Decay types
    kUndefinedDecayType = 0
    kTauToHadDecay = 1
    kTauToElecDecay = 2
    kTauToMuDecay = 3

    def __init__(self, type = 0, pt = 0., eta = 0., phi = 0., mass = 0., decayMode = -1, full_setup = True):
        self.type = type
        self.pt = pt
        self.eta = eta
        self.phi = phi
        self.mass = mass
        self.decayMode = decayMode
        self.initialize()
        
        ###KOMENTARZ###
        #Zapisałem to tak, bo w pierwszym konstruktorze istnieją trzy inicjatory:
        #1. Domyślny, generujący "puste" zdarzenie
        #2. Główny, generujący wszystkie
        #3. Służący do kopiowania

        #Więc oczywiście 2. potrzeba będzie zaimplementować, ale nie wiem czy potrzebujecie pozostałych dwóch
        #i która opcja powinna być domyślna (full_setup = True czy False)

        #W przypadku kopii (3) może też pojawić się problem z tworzeniem kopii - tzn. czy wystarczyłaby zwykła kopia
        #czy jednak potrzeba by było kopii głębokiej i do tego importować odpowiednią bibliotekę
        #(a nwm jak to działa z jit lub numbą)
        #A nie widziałem użycia (3) w kodzie fastMTT, więc może nie trzeba
        ###KONIEC###

        if full_setup:
            minVisMass = electronMass
            maxVisMass = tauLeptonMass

            if self.type == MeasuredTauLepton.kTauToElecDecay:
                minVisMass = electronMass
                maxVisMass = minVisMass
            elif self.type == MeasuredTauLepton.kTauToMuDecay:
                minVisMass = muonMass
                maxVisMass = minVisMass
            elif self.type == MeasuredTauLepton.kTauToHadDecay:
                if self.decayMode == -1:
                    minVisMass = chargedPionMass
                    maxVisMass = 1.5
                elif self.decayMode == 0:
                    minVisMass = chargedPionMass
                    maxVisMass = minVisMass

                elif self.decayMode == 1:
                    minVisMass = 0.3
                    maxVisMass = 1.5

            self.preciseVisMass = self.mass
            if self.type == MeasuredTauLepton.kTauToElecDecay:
                type_string = "tau -> electron decay"
            elif self.type == MeasuredTauLepton.kTauToMuDecay:
                type_string = "tau -> muon decay"
            elif self.type == MeasuredTauLepton.kTauToHadDecay:
                type_string = "tau -> had decay"
            else:
                raise ValueError(f"Invalid type {self.type} declared for leg: Pt = {self.pt}, eta = {self.eta}, phi = {self.phi}, mass = {self.mass} !!")
            print(f"Warning: {type_string} declared for leg: Pt = {self.pt}, eta = {self.eta}, phi = {self.phi}, mass = {self.mass} !!")
            print(f" (mass expected in the range = {minVisMass}..{maxVisMass})")

            if self.preciseVisMass < minVisMass:
                self.preciseVisMass = minVisMass
            if self.preciseVisMass > maxVisMass:
                self.preciseVisMass = maxVisMass

        return
    

    #A to jest wydzielone, bo zachowuję strukturę z oryginalnego kodu
    def initialize(self):
        self.p = self.pt * np.cosh(self.eta)
        self.px = self.pt * np.cos(self.phi)
        self.py = self.pt * np.sin(self.phi)
        self.pz = self.pt * np.sinh(self.eta)
        self.energy = np.sqrt(self.p * self.p + self.mass * self.mass)
        self.p4 = np.array([self.px, self.py, self.pz, self.energy])
        self.p3 = np.array([self.px, self.py, self.pz])
        theta = np.arccos(self.pz / self.p)
        self.cosPhi_sinTheta = np.cos(self.phi) * np.sin(theta)
        self.sinPhi_sinTheta = np.sin(self.phi) * np.sin(theta)
        self.cosTheta = np.cos(theta)