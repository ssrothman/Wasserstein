//------------------------------------------------------------------------
// This file is part of Wasserstein, a C++ library with a Python wrapper
// that computes the Wasserstein/EMD distance. If you use it for academic
// research, please cite or acknowledge the following works:
//
//   - Komiske, Metodiev, Thaler (2019) arXiv:1902.02346
//       https://doi.org/10.1103/PhysRevLett.123.041801
//   - Komiske, Metodiev, Thaler (2020) arXiv:2004.04159
//       https://doi.org/10.1007/JHEP07%282020%29006
//   - Boneel, van de Panne, Paris, Heidrich (2011)
//       https://doi.org/10.1145/2070781.2024192
//   - LEMON graph library https://lemon.cs.elte.hu/trac/lemon
//
// Copyright (C) 2019-2021 Patrick T. Komiske III
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//------------------------------------------------------------------------

#include <cstdlib>

// Helps with reading in events from NumPy .npz files
#include "NPZEventProducer.hh"

// The Wasserstein library
#include "Wasserstein.hh"

using EMDParticle = emd::EuclideanParticle2D<>;
using EMD = emd::EMD<emd::EuclideanEvent2D, emd::EuclideanDistance2D>;
using PairwiseEMD = emd::PairwiseEMD<EMD>;

template<class P>
std::vector<P> convert2event(const std::vector<Particle> & particles) {
  std::vector<P> euclidean_particles;
  euclidean_particles.reserve(particles.size());
  for (const Particle & particle : particles)
    euclidean_particles.push_back(P(particle.pt, {particle.y, particle.phi}));
  return euclidean_particles;
}

void SigmaMD_single(EventProducer * evp) {

  double EMD_R = 0.4;
  double EMD_beta = 1;
  bool EMD_norm = true;
  PairwiseEMD pairwise_emd_obj(EMD_R, EMD_beta, EMD_norm);

  // preprocess events to center
  pairwise_emd_obj.preprocess<emd::CenterWeightedCentroid>();

  // print description
  std::cout << pairwise_emd_obj.description() << std::endl;

  // get vector of events
  std::vector<std::vector<EMDParticle>> events;

  // loop over events and compute the EMD between each successive pair
  evp->reset();
  while (evp->next())
    events.push_back(convert2event<EMDParticle>(evp->particles()));

  // run computation
  pairwise_emd_obj(events.begin(), events.begin() + evp->num_accepted()/2,
                   events.begin() + evp->num_accepted()/2, events.end());

  // get max and min EMD value
  const std::vector<double> & emds_raw(pairwise_emd_obj.emds());
  std::cout << "Min. EMD - " << *std::min_element(emds_raw.begin(), emds_raw.end()) << '\n'
            << "Max. EMD - " << *std::max_element(emds_raw.begin(), emds_raw.end()) << '\n'
            << emds_raw.size() << " emds\n"
            << '\n';

  // setup EMD object to compute cross section mover's distance
  double SigmaMD_R = 1;
  double SigmaMD_beta = 1;
  bool SigmaMD_norm = true;
  bool SigmaMD_do_timing = true;

  // external dists are used by the default configuration
  emd::EMD<> sigmamd_obj(SigmaMD_R, SigmaMD_beta, SigmaMD_norm, SigmaMD_do_timing);

  std::cout << sigmamd_obj.description() << '\n';

  // set distances
  auto emds(pairwise_emd_obj.emds());
  sigmamd_obj.ground_dists().resize(emds.size());
  for (std::size_t i = 0; i < emds.size(); i++)
    sigmamd_obj.ground_dists()[i] = emds[i];

  // form datasets
  std::vector<double> weights0(pairwise_emd_obj.nevA(), 1),
                      weights1(pairwise_emd_obj.nevB(), 1);

  std::cout << "Running computation ..." << std::endl;

  // run computation
  std::cout << "Cross-section Mover's Distance : " << sigmamd_obj(weights0, weights1) << '\n'
            << "Done in " << sigmamd_obj.duration() << "s\n";
}

EventProducer * load_events(int argc, char** argv) {

  // get number of events from command line
  long num_events(1000);
  EventType evtype(All);
  if (argc >= 2)
    num_events = atol(argv[1]);
  if (argc >= 3)
    evtype = atoi(argv[2]) == 1 ? Quark : Gluon;

  // get energyflow samples
  const char * home(std::getenv("HOME"));
  if (home == NULL)
    throw std::invalid_argument("Error: cannot get HOME environment variable");

  // form path
  std::string filepath(home);
  filepath += "/.energyflow/datasets/QG_jets.npz";
  std::cout << "Filepath: " << filepath << '\n';

  // open file
  NPZEventProducer * npz(nullptr);
  try {
    npz = new NPZEventProducer(filepath, num_events, evtype);
  }
  catch (std::exception & e) {
    std::cerr << "Error: cannot open file " << filepath << ", try running "
              << "`python3 -c \"import energyflow as ef; ef.qg_jets.load()\"`\n";
    return nullptr;
  }

  return npz;
}

int main(int argc, char** argv) {

  // load events
  EventProducer * evp(load_events(argc, argv));
  if (evp == nullptr)
    return 1;

  // demonstrate some EMD usage
  SigmaMD_single(evp);

  return 0;
}