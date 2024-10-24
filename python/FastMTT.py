import numpy as np
from MeasuredTauLepton import *

###Reference: https://github.com/SVfit/ClassicSVfit/blob/fastMTT_2024/src/FastMTT.cc ###

class Likelihood:
    def __init__(self, Leptonparams, aMET, aCovMET, aPars = np.array([6, 1/1.15])):
        #Tu zdefiniujemy wszystkie parametry funkcji (self.X)
        #oraz zrobimy inicjalizację Leptonparams zgodnie z funkcją setLeptonInputs z oryginalnego kodu
        #(chyba, że dla przejrzystości lepiej to zrobić w osobnej funkcji i wkleić)
        return
    
    def massLikelihood(self, m):
        #Implementacja
        return #value
    
    def ptLikelihood(self, pTTauTau, type):
        #Implementacja
        return #value
    
    def metTF(metP4, nuP4, covMET):
        #implementacja
        return #value
    
    #Uwaga#
    #ComponentParams zastąpią enable/disable Component
    #które (jeśli dobrze rozumiem) uwzględniają lub nie konkretne prawdopodobieństwa (pt, metTF, mass) w końcowym wyniku
    #Możemy to zrobić w value albo w __init__ - jeszcze nie wiem, co jest potrzebne do poprawnego działania kodu

    def value(self, x, ComponentsParams = True):
        #Implementacja
        #zawiera odwołania do massLikelihood, ptLikelihood, metTF
        #o ile są odblokowane przy definicji funkcji
        return

###UWAGA###
#numba nie obsługuje dziedziczenia funkcji
#więc preferowanym kompilatorem będzie chyba jednak jit
#Tym niemniej przepisanie kodu do jednej klasy też nie powinno być ewentualnie problemem (prędzej dla MeasuredTauLepton niż dla likelihood)
#jeśli performance z numbą byłby znacznie większy
#więc domyślnie napisałbym kod z jit, a potem ew. sprawdził jak to działa z numbą
#(jeśli uznamy, że implementacja nie jest zbyt ciężka w porównaniu do wyniku)

class FastMTT(Likelihood):
    def __init__(self):
        #inicjalizacja
        #parametry aPars i ComponentParams przez dziedziczenie z Likelihood
        #Odpuścimy inicjalizację rzeczy do minimalizacji, skoro i tak jej nie ma w kodzie
        return
    
    def run(self, measuredTauLeptons: np.ndarray, measuredMETx, measuredMETy, covMET) -> np.ndarray:
        return
    
    def compareLeptons(self, measuredTauLepton1: MeasuredTauLepton, measuredTauLepton2: MeasuredTauLepton): #używane w run
        #Implementacja
        return
    
    def scan(self): #używane w run
        #Używa MyLikelihood.value
        #Implementacja z pętlami
        #lub gradient_descent (w artykule było wspomniane, że może poprawić szybkość algorytmu)
        return
    
#Do wszystkiego dołożymy do testowania funkcje do pomiaru czasu (np. import time), tak jak w oryginalnym kodzie