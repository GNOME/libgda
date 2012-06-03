/*
 * Determines that java works correctly and get its libraries path
 * (originally from the R project)
 */ 
public class getsp {
	public static void main(String[] args) {
		if (args!=null && args.length>0) {
			if (args[0].compareTo("-test")==0) {
				System.out.println("Test1234OK");
			}
			else if (args[0].compareTo("-libs")==0) {
				String prefix="-L";
				if (args.length>1) prefix=args[1];

				// we're not using StringTokenizer in case the JVM is very crude
				int i=0,j,k=0;
				String r=null;
				String pss=System.getProperty("path.separator");
				char ps=':';
				if (pss!=null && pss.length()>0) ps=pss.charAt(0);

				// using java.library.path
				String lp=System.getProperty("java.home");
				j=lp.length();
				while (i<=j) {
					if (i==j || lp.charAt(i)==ps) {
						String lib=lp.substring(k,i);
						String suffix="/lib/amd64/server";
						k=i+1;
						if (lib.compareTo(".")!=0)
							r=(r==null)?(prefix+lib+suffix):(r+" "+prefix+lib+suffix);
					}
					i++;
				}

				// using java.home
				lp=System.getProperty("java.library.path");
				j=lp.length();
				i=0;
				k=0;
				while (i<=j) {
					if (i==j || lp.charAt(i)==ps) {
						String lib=lp.substring(k,i);
						k=i+1;
						if (lib.compareTo(".")!=0)
							r=(r==null)?(prefix+lib):(r+" "+prefix+lib);
					}
					i++;
				}

				if (r!=null) System.out.println(r);
			} else if (args[0].compareTo("-ldpath")==0) {
				String lp1=System.getProperty("java.home")+"/lib/amd64/server";
				String lp2=System.getProperty("java.library.path");
				System.out.println(lp1+":"+lp2);
			}
			else
				System.out.println(System.getProperty(args[0]));
		}
	}
}
